
#include "MiSpiAnalyzer.h"
#include "MiSpiAnalyzerSettings.h"

#include <AnalyzerChannelData.h>


// enum SpiBubbleType { SpiData, SpiError };

MiSpiAnalyzer::MiSpiAnalyzer()
    : Analyzer2(),
      mSettings( new MiSpiAnalyzerSettings() ),
      mSimulationInitilized( false ),
      mData( NULL ),
      mClock( NULL )
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
    U32 mSyncHighUs = 270 - mStartTol;
    U32 mClockTimeoutUs = 300;

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
        U64 clock_duration_us = clock_length_samples * ( 1000000 / mSampleRateHz );

        if (clock_duration_us > mClockTimeoutUs) {
            // Invalid pulse, let's reset the state machine
            bit_count = 0;
            data = 0;
            direction = MiSpiDirUnknown;
            mResults->CancelPacketAndStartNewPacket();

            Frame frame;
            frame.mFlags = 0;
            frame.mData1 = 0;
            frame.mType = MiSpiError;
            FinalizeFrame(frame, clock_start, clock_end);

        } else if (clock_duration_us > mSyncHighUs) {
            // Record Sync Pulse, reset state machine
            bit_count = 0;
            data = 0;
            direction = MiSpiDirUnknown;
            mResults->CancelPacketAndStartNewPacket();

            Frame frame;
            frame.mFlags = 0;
            frame.mData1 = 0;
            frame.mType = MiSpiSync;
            FinalizeFrame(frame, clock_start, clock_end);

            mResults->AddFrameV2(framev2, "Sync", clock_start, clock_end);
            mResults->CommitResults();

        } else if (clock_duration_us > mStartMosiHighUs) {
            // Record MOSI start

            // Add Marker
            mResults->AddMarker(clock_end, AnalyzerResults::Start, mSettings->mClockChannel);

            // Reset byte data
            bit_count = 0;
            data = 0;

            // Packet API is currently broken

            // Commit everything before this as a packet IF state is valid
            // if (direction != MiSpiDirUnknown) {
            // mResults->CommitPacketAndStartNewPacket();
            // } else {
                // mResults->CancelPacketAndStartNewPacket();
            // }
            // mResults->CommitResults();

            // Update direction
            direction = MiSpiDirMosi;

            // Configure frame v1
            Frame frame;
            frame.mFlags = 0;
            frame.mData1 = 0;
            frame.mType = MiSpiStartMosi;
            FinalizeFrame(frame, clock_start, clock_end);

            // Configure frame v2
            framev2.AddString("Direction", "MOSI");
            mResults->AddFrameV2(framev2, "Start", clock_start, clock_end);
            mResults->CommitResults();
        } else if (clock_duration_us > mStartMisoHighUs) {
            // Record MISO start

            // Add Marker
            mResults->AddMarker(clock_end, AnalyzerResults::Stop, mSettings->mClockChannel);

            // Reset byte data
            bit_count = 0;
            data = 0;

            // Commit everything before this as a packet IF state is valid
            // if (direction != MiSpiDirUnknown) {
                // mResults->CommitPacketAndStartNewPacket();
            // } else {
                // mResults->CancelPacketAndStartNewPacket();
            // }
            // mResults->CommitResults();

            // Update direction
            direction = MiSpiDirMiso;

            // Configure frame v1
            Frame frame;
            frame.mFlags = 0;
            frame.mData1 = 0;
            frame.mType = MiSpiStartMiso;
            FinalizeFrame(frame, clock_start, clock_end);

            // Configure frame v2
            framev2.AddString("Direction", "MISO");
            mResults->AddFrameV2(framev2, "Start", clock_start, clock_end);
            mResults->CommitResults();
        } else {
            // Record bit

            // Add Marker
            mResults->AddMarker(clock_end, AnalyzerResults::DownArrow, mSettings->mClockChannel);

            // Determine if this edge is a 1 or a 0
            mData->AdvanceToAbsPosition(clock_end);
            if( mData->GetBitState() == BIT_HIGH ) {
                if (mSettings->mShiftOrder == AnalyzerEnums::MsbFirst) {
                    data |= (128 >> bit_count);
                } else {
                    data |= (1 << bit_count);
                }
            } 

            // Handle starting a new byte
            if (bit_count == 0) {
                byte_start = clock_start;
            }

            bit_count++;

            // Handle ending a byte
            if (bit_count == 8) {
                // Frame v1
                Frame frame;
                frame.mFlags = 0;
                frame.mData1 = data; 
                frame.mType = MiSpiData;
                FinalizeFrame(frame, byte_start, clock_end);

                // Frame v2
                framev2.AddByte("Data", data);
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

void MiSpiAnalyzer::FinalizeFrame(Frame frame, U64 start, U64 end)
{
    frame.mStartingSampleInclusive = start;
    frame.mEndingSampleInclusive = end; 
    mResults->AddFrame(frame);
    mResults->CommitResults();
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
