#ifndef SPI_ANALYZER_RESULTS
#define SPI_ANALYZER_RESULTS

#include <AnalyzerResults.h>

#define SPI_ERROR_FLAG ( 1 << 0 )

enum MiSpiFrameType {
  MiSpiStartMiso,
  MiSpiStartMosi,
  MiSpiData
};

class MiSpiAnalyzer;
class MiSpiAnalyzerSettings;

class MiSpiAnalyzerResults : public AnalyzerResults
{
  public:
    MiSpiAnalyzerResults( MiSpiAnalyzer* analyzer, MiSpiAnalyzerSettings* settings );
    virtual ~MiSpiAnalyzerResults();

    virtual void GenerateBubbleText( U64 frame_index, Channel& channel, DisplayBase display_base );
    virtual void GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id );

    virtual void GenerateFrameTabularText( U64 frame_index, DisplayBase display_base );
    virtual void GeneratePacketTabularText( U64 packet_id, DisplayBase display_base );
    virtual void GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base );

  protected: // functions
  protected: // vars
    MiSpiAnalyzerSettings* mSettings;
    MiSpiAnalyzer* mAnalyzer;
};

#endif // SPI_ANALYZER_RESULTS
