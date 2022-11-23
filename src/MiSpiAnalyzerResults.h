#ifndef SPI_ANALYZER_RESULTS
#define SPI_ANALYZER_RESULTS

#include <AnalyzerResults.h>

#define SPI_ERROR_FLAG ( 1 << 0 )

enum MiSpiFrameType {
  MiSpiStartMiso,
  MiSpiStartMosi,
  MiSpiData,
  MiSpiError
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
    void SubmitFrame(Frame frame);
    void SubmitMisoPacket(void *f, DisplayBase display_base);
    void SubmitMosiPacket(void *f, DisplayBase display_base);
    void ClosePackets(void *f, DisplayBase display_base);
  protected: // vars
    MiSpiAnalyzerSettings* mSettings;
    MiSpiAnalyzer* mAnalyzer;
    std::vector<U64> mosi_packet;
    std::vector<U64> miso_packet;
    std::vector<U64> new_packet;
    U8 mosi_reps;
    U8 miso_reps;
};

#endif // SPI_ANALYZER_RESULTS
