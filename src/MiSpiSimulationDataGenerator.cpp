#include "MiSpiSimulationDataGenerator.h"
#include "MiSpiAnalyzerSettings.h"

MiSpiSimulationDataGenerator::MiSpiSimulationDataGenerator()
{
}

MiSpiSimulationDataGenerator::~MiSpiSimulationDataGenerator()
{
}

void MiSpiSimulationDataGenerator::Initialize( U32 simulation_sample_rate, MiSpiAnalyzerSettings* settings )
{
    mSimulationSampleRateHz = simulation_sample_rate;
    mSettings = settings;

    mClockGenerator.Init( simulation_sample_rate / 10, simulation_sample_rate );

    mData = mMiSpiSimulationChannels.Add( settings->mDataChannel, mSimulationSampleRateHz, BIT_LOW );
    mClock = mMiSpiSimulationChannels.Add( settings->mClockChannel, mSimulationSampleRateHz, mSettings->mClockInactiveState );

    if( settings->mEnableChannel != UNDEFINED_CHANNEL )
        mEnable = mMiSpiSimulationChannels.Add( settings->mEnableChannel, mSimulationSampleRateHz, Invert( mSettings->mEnableActiveState ) );
    else
        mEnable = NULL;

    mMiSpiSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( 10.0 ) ); // insert 10 bit-periods of idle

    mValue = 0;

    mDirection = 1; // 1 = MISO block, -1 = MOSI block
}

U32 MiSpiSimulationDataGenerator::GenerateSimulationData( U64 largest_sample_requested, U32 sample_rate,
                                                        SimulationChannelDescriptor** simulation_channels )
{
    U64 adjusted_largest_sample_requested =
        AnalyzerHelpers::AdjustSimulationTargetSample( largest_sample_requested, sample_rate, mSimulationSampleRateHz );

    while( mClock->GetCurrentSampleNumber() < adjusted_largest_sample_requested )
    {
        CreateSpiTransaction();

        mMiSpiSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( 10.0 ) ); // insert 10 bit-periods of idle
    }

    *simulation_channels = mMiSpiSimulationChannels.GetArray();
    return mMiSpiSimulationChannels.GetCount();
}

void MiSpiSimulationDataGenerator::CreateSpiTransaction()
{
    if( mEnable != NULL )
        mEnable->Transition();

    mMiSpiSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( 2.0 ) );

    OutputInit( mDirection );
    mDirection = mDirection * -1;

    if( mSettings->mDataValidEdge == AnalyzerEnums::LeadingEdge )
    {
        OutputWord_CPHA0( mValue );
        mValue++;

        OutputWord_CPHA0( mValue );
        mValue++;

        OutputWord_CPHA0( mValue );
        mValue++;

        if( mEnable != NULL )
            mEnable->Transition();

        OutputWord_CPHA0( mValue );
        mValue++;
    }
    else
    {
        OutputWord_CPHA1( mValue );
        mValue++;

        OutputWord_CPHA1( mValue );
        mValue++;

        OutputWord_CPHA1( mValue );
        mValue++;

        if( mEnable != NULL )
            mEnable->Transition();

        OutputWord_CPHA1( mValue );
        mValue++;
    }
}

void MiSpiSimulationDataGenerator::OutputInit( int direction )
{

    mClock->TransitionIfNeeded( BIT_LOW );  // Pull clock low, if it isn't already
    mMiSpiSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByTimeS( 0.0004 ) ); // Hold for 400us
    mClock->Transition(); // Pull clock high
    if (direction > 0) {
        mMiSpiSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByTimeS( 0.00009 ) ); // Hold for 90us
    } else {
        mMiSpiSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByTimeS( 0.00016 ) ); // Hold for 160us
    }
    mClock->Transition(); // Pull clock low
    mMiSpiSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByTimeS( 0.0004 ) ); // Hold for 400us

}

void MiSpiSimulationDataGenerator::OutputWord_CPHA0( U64 mispi_data )
{
    BitExtractor data_bits( mispi_data, mSettings->mShiftOrder, mSettings->mBitsPerTransfer );

    U32 count = mSettings->mBitsPerTransfer;
    for( U32 i = 0; i < count; i++ )
    {
        mData->TransitionIfNeeded( data_bits.GetNextBit() );

        mMiSpiSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( .5 ) );
        mClock->Transition(); // data valid

        mMiSpiSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( .5 ) );
        mClock->Transition(); // data invalid
    }

    mData->TransitionIfNeeded( BIT_LOW );

    mMiSpiSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( 2.0 ) );
}

void MiSpiSimulationDataGenerator::OutputWord_CPHA1( U64 mispi_data )
{
    BitExtractor data_bits( mispi_data, mSettings->mShiftOrder, mSettings->mBitsPerTransfer );

    U32 count = mSettings->mBitsPerTransfer;
    for( U32 i = 0; i < count; i++ )
    {
        mClock->Transition(); // data invalid
        mData->TransitionIfNeeded( data_bits.GetNextBit() );

        mMiSpiSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( .5 ) );
        mClock->Transition(); // data valid

        mMiSpiSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( .5 ) );
    }

    mData->TransitionIfNeeded( BIT_LOW );

    mMiSpiSimulationChannels.AdvanceAll( mClockGenerator.AdvanceByHalfPeriod( 2.0 ) );
}
