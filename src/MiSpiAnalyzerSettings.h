#ifndef SPI_ANALYZER_SETTINGS
#define SPI_ANALYZER_SETTINGS

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>

class MiSpiAnalyzerSettings : public AnalyzerSettings
{
  public:
    MiSpiAnalyzerSettings();
    virtual ~MiSpiAnalyzerSettings();

    virtual bool SetSettingsFromInterfaces();
    virtual void LoadSettings( const char* settings );
    virtual const char* SaveSettings();

    void UpdateInterfacesFromSettings();

    // Channel mMosiChannel;
    // Channel mMisoChannel;
    Channel mDataChannel;
    Channel mClockChannel;
    AnalyzerEnums::ShiftOrder mShiftOrder;


  protected:
    // std::auto_ptr<AnalyzerSettingInterfaceChannel> mMosiChannelInterface;
    // std::auto_ptr<AnalyzerSettingInterfaceChannel> mMisoChannelInterface;
    std::auto_ptr<AnalyzerSettingInterfaceChannel> mDataChannelInterface;
    std::auto_ptr<AnalyzerSettingInterfaceChannel> mClockChannelInterface;
    std::auto_ptr<AnalyzerSettingInterfaceNumberList> mShiftOrderInterface;
};

#endif // SPI_ANALYZER_SETTINGS
