#ifndef SPI_SIMULATION_DATA_GENERATOR
#define SPI_SIMULATION_DATA_GENERATOR

#include <AnalyzerHelpers.h>

class MiSpiAnalyzerSettings;

class MiSpiSimulationDataGenerator
{
  public:
    MiSpiSimulationDataGenerator();
    ~MiSpiSimulationDataGenerator();

    void Initialize( U32 simulation_sample_rate, MiSpiAnalyzerSettings* settings );
    U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels );

  protected:
    MiSpiAnalyzerSettings* mSettings;
    U32 mSimulationSampleRateHz;
    U64 mValue;
    int mDirection;

  protected: // SPI specific
    ClockGenerator mClockGenerator;

    void CreateSpiTransaction();
    void OutputInit( int direction );
    void OutputWord( U64 mispi_data );


    SimulationChannelDescriptorGroup mMiSpiSimulationChannels;
    SimulationChannelDescriptor* mData;
    SimulationChannelDescriptor* mClock;
};
#endif // SPI_SIMULATION_DATA_GENERATOR
