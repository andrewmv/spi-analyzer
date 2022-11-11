#ifndef SPI_ANALYZER_H
#define SPI_ANALYZER_H

#include <Analyzer.h>
#include "MiSpiAnalyzerResults.h"
#include "MiSpiSimulationDataGenerator.h"

class MiSpiAnalyzerSettings;
class MiSpiAnalyzer : public Analyzer2
{
  public:
    MiSpiAnalyzer();
    virtual ~MiSpiAnalyzer();
    virtual void SetupResults();
    virtual void WorkerThread();

    virtual U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels );
    virtual U32 GetMinimumSampleRateHz();

    virtual const char* GetAnalyzerName() const;
    virtual bool NeedsRerun();

  protected: // functions
    void Setup();
    void AdvanceToActiveEnableEdge();
    bool IsInitialClockPolarityCorrect();
    void AdvanceToActiveEnableEdgeWithCorrectClockPolarity();
    bool WouldAdvancingTheClockToggleEnable( bool add_disable_frame, U64* disable_frame );
    void GetWord();

#pragma warning( push )
#pragma warning(                                                                                                                           \
    disable : 4251 ) // warning C4251: 'SerialAnalyzer::<...>' : class <...> needs to have dll-interface to be used by clients of class
  protected:         // vars
    std::auto_ptr<MiSpiAnalyzerSettings> mSettings;
    std::auto_ptr<MiSpiAnalyzerResults> mResults;
    bool mSimulationInitilized;
    MiSpiSimulationDataGenerator mSimulationDataGenerator;

    AnalyzerChannelData* mData;
    AnalyzerChannelData* mClock;
    AnalyzerChannelData* mEnable;

    U64 mCurrentSample;
    AnalyzerResults::MarkerType mArrowMarker;
    std::vector<U64> mArrowLocations;
    DataBuilder mDataResult;


#pragma warning( pop )
};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer();
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer( Analyzer* analyzer );

#endif // SPI_ANALYZER_H
