
#include "MiSpiAnalyzer.h"
#include "MiSpiAnalyzerSettings.h"

#include <AnalyzerChannelData.h>


// enum SpiBubbleType { SpiData, SpiError };

MiSpiAnalyzer::MiSpiAnalyzer()
    : Analyzer2(),
      mSettings( new MiSpiAnalyzerSettings() ),
      mSimulationInitilized( false ),
      mData( NULL ),
      mClock( NULL ),
      mEnable( NULL )
{
    SetAnalyzerSettings( mSettings.get() );
    UseFrameV2();
}

MiSpiAnalyzer::~MiSpiAnalyzer()
{
    KillThread();
}

void MiSpiAnalyzer::SetupResults()
{
    mResults.reset( new MiSpiAnalyzerResults( this, mSettings.get() ) );
    SetAnalyzerResults( mResults.get() );

    mResults->AddChannelBubblesWillAppearOn( mSettings->mDataChannel );
}

void MiSpiAnalyzer::WorkerThread()
{
    // Setup
    // We're ignoring Enable, CPOL, and CPHA settings for now - AMV
    mData = GetAnalyzerChannelData( mSettings->mDataChannel );
    mClock = GetAnalyzerChannelData( mSettings->mClockChannel );
    U32 mSampleRateHz = GetSampleRate();

    // TODO - Variablize these
    U32 mStartTol = 20;
    U32 mStartMisoHighUs = 90 - mStartTol;
    U32 mStartMosiHighUs = 160 - mStartTol;
    U32 mStartLowUs = 400;
    U32 mBitHighUs = 8;
    U32 mBitLowUs = 8;

    // State machine variables
    U8 bit_count = 0;
    U8 data = 0;
    U64 byte_start = 0;
    MiSpiDirection direction = MiSpiDirUnknown;

    // Wait for the clock to go low before we start analyzing anything
    if( mClock->GetBitState() == BIT_HIGH )
        mClock->AdvanceToNextEdge();

    for( ; ; )
    {
        // setup legacy frame for bubbles
        Frame frame;
        frame.mFlags = 0;
        frame.mData1 = 0;
        
        // setup v2 frame for tables
        FrameV2 framev2;

        // Get the next clock pulse
        mClock->AdvanceToNextEdge(); //leading edge
        U64 clock_start = mClock->GetSampleNumber();
        mClock->AdvanceToNextEdge(); //trailing edge
        U64 clock_end = mClock->GetSampleNumber();

        // Move the progress bar along
        ReportProgress( clock_end ); 

        // How long was that?
        U64 clock_length_samples = clock_end - clock_start;
        U64 clock_duration_us = clock_length_samples * (mSampleRateHz / 1000000);

        if (clock_duration_us > mStartMosiHighUs) {
            // Record MOSI start

            // Reset byte data
            bit_count = 0;
            data = 0;

            // Update direction
            direction = MiSpiDirMosi;

            // Configure frame v1
            frame.mType = MiSpiStartMosi;
            FinalizeFrame(frame, clock_start, clock_end);

            // Configure frame v2
            // AnnotateDirection(framev2, direction);
            framev2.AddString("Direction", "MOSI");
            mResults->AddFrameV2(framev2, "Start", clock_start, clock_end);
            mResults->CommitResults();
        } else if (clock_duration_us > mStartMisoHighUs) {
            // Record MISO start

            // Reset byte data
            bit_count = 0;
            data = 0;

            // Update direction
            direction = MiSpiDirMiso;

            // Configure frame v1
            frame.mType = MiSpiStartMiso;
            FinalizeFrame(frame, clock_start, clock_end);

            // Configure frame v2
            // AnnotateDirection(framev2, direction);
            framev2.AddString("Direction", "MISO");
            mResults->AddFrameV2(framev2, "Start", clock_start, clock_end);
            mResults->CommitResults();
        } else {
            // Record bit

            // Determine if this edge is a 1 or a 0
            mData->AdvanceToAbsPosition(clock_end);
            if( mData->GetBitState() == BIT_HIGH ) {
               data |= (1 << bit_count);
            } 

            // Handle starting a new byte
            if (bit_count == 0) {
                byte_start = clock_start;
            }

            bit_count++;

            // Handle ending a byte
            if (bit_count == 8) {
                // Frame v1
                frame.mData1 = data; 
                frame.mType = MiSpiData;
                FinalizeFrame(frame, byte_start, clock_end);

                // Frame v2
                framev2.AddByte("Data", data);
                // AnnotateDirection(framev2, direction);
                if (direction == MiSpiDirMiso) {
                    framev2.AddString("Direction", "MISO");
                } else if (direction == MiSpiDirMosi) {
                    framev2.AddString("Direction", "MOSI");
                } else {
                    framev2.AddString("Direction", "Unknown");
                } 
                mResults->AddFrameV2(framev2, "Data", byte_start, clock_end);
                mResults->CommitResults();

                // Reset byte data
                bit_count = 0;
                data = 0;
            }
        }
    }
}

void MiSpiAnalyzer::AnnotateDirection(FrameV2 framev2, MiSpiDirection dir)
{
    if (dir == MiSpiDirMiso) {
        framev2.AddString("Direction", "MISO");
    } else if (dir == MiSpiDirMosi) {
        framev2.AddString("Direction", "MOSI");
    } else {
        framev2.AddString("Direction", "Unknown");
    }
}

void MiSpiAnalyzer::FinalizeFrame(Frame frame, U64 start, U64 end)
{
    frame.mStartingSampleInclusive = start;
    frame.mEndingSampleInclusive = end; 
    mResults->AddFrame(frame);
    mResults->CommitResults();
}

void MiSpiAnalyzer::FinalizeFrameV2(FrameV2 framev2, U64 start, U64 end)
{
    mResults->AddFrameV2(framev2, "start", start, end);
    mResults->CommitResults();
}

void MiSpiAnalyzer::AdvanceToActiveEnableEdgeWithCorrectClockPolarity()
{
    mResults->CommitPacketAndStartNewPacket();
    mResults->CommitResults();

    AdvanceToActiveEnableEdge();

    for( ;; )
    {
        if( IsInitialClockPolarityCorrect() == true ) // if false, this function moves to the next active enable edge.
        {
            if( mEnable )
            {
                FrameV2 frame_v2_start_of_transaction;
                mResults->AddFrameV2( frame_v2_start_of_transaction, "enable", mCurrentSample, mCurrentSample + 1 );
            }
            break;
        }
    }
}

void MiSpiAnalyzer::Setup()
{
    bool allow_last_trailing_clock_edge_to_fall_outside_enable = false;
    if( mSettings->mDataValidEdge == AnalyzerEnums::LeadingEdge )
        allow_last_trailing_clock_edge_to_fall_outside_enable = true;

    if( mSettings->mClockInactiveState == BIT_LOW )
    {
        if( mSettings->mDataValidEdge == AnalyzerEnums::LeadingEdge )
            mArrowMarker = AnalyzerResults::UpArrow;
        else
            mArrowMarker = AnalyzerResults::DownArrow;
    }
    else
    {
        if( mSettings->mDataValidEdge == AnalyzerEnums::LeadingEdge )
            mArrowMarker = AnalyzerResults::DownArrow;
        else
            mArrowMarker = AnalyzerResults::UpArrow;
    }

    mData = GetAnalyzerChannelData( mSettings->mDataChannel );
    mClock = GetAnalyzerChannelData( mSettings->mClockChannel );

    if( mSettings->mEnableChannel != UNDEFINED_CHANNEL )
        mEnable = GetAnalyzerChannelData( mSettings->mEnableChannel );
    else
        mEnable = NULL;
}

void MiSpiAnalyzer::AdvanceToActiveEnableEdge()
{
    if( mEnable != NULL )
    {
        if( mEnable->GetBitState() != mSettings->mEnableActiveState )
        {
            mEnable->AdvanceToNextEdge();
        }
        else
        {
            mEnable->AdvanceToNextEdge();
            mEnable->AdvanceToNextEdge();
        }
        mCurrentSample = mEnable->GetSampleNumber();
        mClock->AdvanceToAbsPosition( mCurrentSample );
    }
    else
    {
        mCurrentSample = mClock->GetSampleNumber();
    }
}

bool MiSpiAnalyzer::IsInitialClockPolarityCorrect()
{
    if( mClock->GetBitState() == mSettings->mClockInactiveState )
        return true;

    mResults->AddMarker( mCurrentSample, AnalyzerResults::ErrorSquare, mSettings->mClockChannel );

    if( mEnable != NULL )
    {
        Frame error_frame;
        error_frame.mStartingSampleInclusive = mCurrentSample;

        mEnable->AdvanceToNextEdge();
        mCurrentSample = mEnable->GetSampleNumber();

        error_frame.mEndingSampleInclusive = mCurrentSample;
        error_frame.mFlags = SPI_ERROR_FLAG | DISPLAY_AS_ERROR_FLAG;
        mResults->AddFrame( error_frame );

        FrameV2 framev2;
        mResults->AddFrameV2( framev2, "error", error_frame.mStartingSampleInclusive, error_frame.mEndingSampleInclusive + 1 );

        mResults->CommitResults();
        ReportProgress( error_frame.mEndingSampleInclusive );

        // move to the next active-going enable edge
        mEnable->AdvanceToNextEdge();
        mCurrentSample = mEnable->GetSampleNumber();
        mClock->AdvanceToAbsPosition( mCurrentSample );

        return false;
    }
    else
    {
        mClock->AdvanceToNextEdge(); // at least start with the clock in the idle state.
        mCurrentSample = mClock->GetSampleNumber();
        return true;
    }
}

bool MiSpiAnalyzer::WouldAdvancingTheClockToggleEnable( bool add_disable_frame, U64* disable_frame )
{
    if( mEnable == NULL )
        return false;

    auto log_disable_event = [&]( U64 enable_edge ) {
        if( add_disable_frame )
        {
            FrameV2 frame_v2_end_of_transaction;
            mResults->AddFrameV2( frame_v2_end_of_transaction, "disable", enable_edge, enable_edge + 1 );
        }
        else if( disable_frame != nullptr )
        {
            *disable_frame = enable_edge;
        }
    };

    // if the enable is currently active, and there are no more clock transitions in the capture, attempt to capture the final disable event
    if( !mClock->DoMoreTransitionsExistInCurrentData() && mEnable->GetBitState() == mSettings->mEnableActiveState )
    {
        if( mEnable->DoMoreTransitionsExistInCurrentData() )
        {
            U64 next_enable_edge = mEnable->GetSampleOfNextEdge();
            // double check that the clock line actually processed all samples up to the next enable edge.
            // double check is required becase data is getting processed while we're running, it's possible more has already become
            // available.
            if( !mClock->WouldAdvancingToAbsPositionCauseTransition( next_enable_edge ) )
            {
                log_disable_event( next_enable_edge );
                return true;
            }
        }
    }

    U64 next_edge = mClock->GetSampleOfNextEdge();
    bool enable_will_toggle = mEnable->WouldAdvancingToAbsPositionCauseTransition( next_edge );

    if( enable_will_toggle )
    {
        U64 enable_edge = mEnable->GetSampleOfNextEdge();
        log_disable_event( enable_edge );
    }

    if( enable_will_toggle == false )
        return false;
    else
        return true;
}

void MiSpiAnalyzer::GetWord()
{
    // we're assuming we come into this function with the clock in the idle state;

    const U32 bits_per_transfer = mSettings->mBitsPerTransfer;
    const U32 bytes_per_transfer = ( bits_per_transfer + 7 ) / 8;

    U64 data_word = 0;
    mDataResult.Reset( &data_word, mSettings->mShiftOrder, bits_per_transfer );

    U64 first_sample = 0;
    bool need_reset = false;
    U64 disable_event_sample = 0;


    mArrowLocations.clear();
    ReportProgress( mClock->GetSampleNumber() );

    for( U32 i = 0; i < bits_per_transfer; i++ )
    {
        if( i == 0 )
            CheckIfThreadShouldExit();

        // on every single edge, we need to check that enable doesn't toggle.
        // note that we can't just advance the enable line to the next edge, becuase there may not be another edge

        if( WouldAdvancingTheClockToggleEnable( true, nullptr ) == true )
        {
            AdvanceToActiveEnableEdgeWithCorrectClockPolarity(); // ok, we pretty much need to reset everything and return.
            return;
        }

        mClock->AdvanceToNextEdge();
        if( i == 0 )
            first_sample = mClock->GetSampleNumber();

        if( mSettings->mDataValidEdge == AnalyzerEnums::LeadingEdge )
        {
            mCurrentSample = mClock->GetSampleNumber();
            mData->AdvanceToAbsPosition( mCurrentSample );
            mDataResult.AddBit( mData->GetBitState() );
            mArrowLocations.push_back( mCurrentSample );
        }


        // ok, the trailing edge is messy -- but only on the very last bit.
        // If the trialing edge isn't doesn't represent valid data, we want to allow the enable line to rise before the clock trialing edge
        // -- and still report the frame
        if( ( i == ( bits_per_transfer - 1 ) ) && ( mSettings->mDataValidEdge != AnalyzerEnums::TrailingEdge ) )
        {
            // if this is the last bit, and the trailing edge doesn't represent valid data
            if( WouldAdvancingTheClockToggleEnable( false, &disable_event_sample ) == true )
            {
                // moving to the trailing edge would cause the clock to revert to inactive.  jump out, record the frame, and them move to
                // the next active enable edge
                need_reset = true;
                break;
            }

            // enable isn't going to go inactive, go ahead and advance the clock as usual.  Then we're done, jump out and record the frame.
            mClock->AdvanceToNextEdge();
            break;
        }

        // this isn't the very last bit, etc, so proceed as normal
        if( WouldAdvancingTheClockToggleEnable( true, nullptr ) == true )
        {
            AdvanceToActiveEnableEdgeWithCorrectClockPolarity(); // ok, we pretty much need to reset everything and return.
            return;
        }

        mClock->AdvanceToNextEdge();

        if( mSettings->mDataValidEdge == AnalyzerEnums::TrailingEdge )
        {
            mCurrentSample = mClock->GetSampleNumber();
            mData->AdvanceToAbsPosition( mCurrentSample );
            mDataResult.AddBit( mData->GetBitState() );
            mArrowLocations.push_back( mCurrentSample );
        }
    }

    // save the results:
    U32 count = mArrowLocations.size();
    for( U32 i = 0; i < count; i++ )
        mResults->AddMarker( mArrowLocations[ i ], mArrowMarker, mSettings->mClockChannel );

    Frame result_frame;
    result_frame.mStartingSampleInclusive = first_sample;
    result_frame.mEndingSampleInclusive = mClock->GetSampleNumber();
    result_frame.mData1 = data_word;
    result_frame.mFlags = 0;
    mResults->AddFrame( result_frame );

    FrameV2 framev2;

    // Max bits per transfer == 64, max bytes == 8
    U8 data_bytearray[ 8 ];
    for( int i = 0; i < bytes_per_transfer; ++i )
    {
        auto bit_offset = ( bytes_per_transfer - i - 1 ) * 8;
        data_bytearray[ i ] = data_word >> bit_offset;
    }
    framev2.AddByteArray( "data", data_bytearray, bytes_per_transfer );

    mResults->AddFrameV2( framev2, "result", first_sample, mClock->GetSampleNumber() + 1 );

    mResults->CommitResults();

    if( need_reset == true )
    {
        FrameV2 frame_v2_end_of_transaction;
        mResults->AddFrameV2( frame_v2_end_of_transaction, "disable", disable_event_sample, disable_event_sample + 1 );
        AdvanceToActiveEnableEdgeWithCorrectClockPolarity();
    }
}

bool MiSpiAnalyzer::NeedsRerun()
{
    return false;
}

U32 MiSpiAnalyzer::GenerateSimulationData( U64 minimum_sample_index, U32 device_sample_rate,
                                         SimulationChannelDescriptor** simulation_channels )
{
    if( mSimulationInitilized == false )
    {
        mSimulationDataGenerator.Initialize( GetSimulationSampleRate(), mSettings.get() );
        mSimulationInitilized = true;
    }

    return mSimulationDataGenerator.GenerateSimulationData( minimum_sample_index, device_sample_rate, simulation_channels );
}


U32 MiSpiAnalyzer::GetMinimumSampleRateHz()
{
    return 125000 * 4;
}

const char* MiSpiAnalyzer::GetAnalyzerName() const
{
    return "MI-SPI";
}

const char* GetAnalyzerName()
{
    return "MI-SPI";
}

Analyzer* CreateAnalyzer()
{
    return new MiSpiAnalyzer();
}

void DestroyAnalyzer( Analyzer* analyzer )
{
    delete analyzer;
}
