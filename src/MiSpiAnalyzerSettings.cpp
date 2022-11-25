#include "MiSpiAnalyzerSettings.h"

#include <AnalyzerHelpers.h>
#include <sstream>
#include <cstring>

MiSpiAnalyzerSettings::MiSpiAnalyzerSettings()
    : mDataChannel( UNDEFINED_CHANNEL ),
      mClockChannel( UNDEFINED_CHANNEL ),
      mShiftOrder( AnalyzerEnums::MsbFirst )
{
    mDataChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
    mDataChannelInterface->SetTitleAndTooltip( "Data", "MOSI/MISO (Multiplexed)" );
    mDataChannelInterface->SetChannel( mDataChannel );
    mDataChannelInterface->SetSelectionOfNoneIsAllowed( false );

    mClockChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
    mClockChannelInterface->SetTitleAndTooltip( "Clock", "Clock (CLK)" );
    mClockChannelInterface->SetChannel( mClockChannel );

    mShiftOrderInterface.reset( new AnalyzerSettingInterfaceNumberList() );
    mShiftOrderInterface->SetTitleAndTooltip( "Significant Bit", "" );
    mShiftOrderInterface->AddNumber( AnalyzerEnums::MsbFirst, "MSB First",
                                     "Select if the most significant bit or least significant bit is transmitted first" );
    mShiftOrderInterface->AddNumber( AnalyzerEnums::LsbFirst, "LSB First", "" );
    mShiftOrderInterface->SetNumber( mShiftOrder );

    AddInterface( mDataChannelInterface.get() );
    AddInterface( mClockChannelInterface.get() );
    AddInterface( mShiftOrderInterface.get() );

    // AddExportOption( 0, "Export as text/csv file", "text (*.txt);;csv (*.csv)" );
    AddExportOption( 0, "Export as CSV file" );
    AddExportExtension( 0, "csv", "csv" );

    ClearChannels();
    AddChannel( mDataChannel, "DATA", false );
    AddChannel( mClockChannel, "CLOCK", false );
}

MiSpiAnalyzerSettings::~MiSpiAnalyzerSettings()
{
}

bool MiSpiAnalyzerSettings::SetSettingsFromInterfaces()
{
    Channel data = mDataChannelInterface->GetChannel();
    Channel clock = mClockChannelInterface->GetChannel();

    std::vector<Channel> channels;
    channels.push_back( data );
    channels.push_back( clock );

    if( AnalyzerHelpers::DoChannelsOverlap( &channels[ 0 ], channels.size() ) == true )
    {
        SetErrorText( "Please select different channels for each input." );
        return false;
    }

    mDataChannel = mDataChannelInterface->GetChannel();
    mClockChannel = mClockChannelInterface->GetChannel();

    mShiftOrder = ( AnalyzerEnums::ShiftOrder )U32( mShiftOrderInterface->GetNumber() );

    ClearChannels();
    AddChannel( mDataChannel, "DATA", mDataChannel != UNDEFINED_CHANNEL );
    AddChannel( mClockChannel, "CLOCK", mClockChannel != UNDEFINED_CHANNEL );

    return true;
}

void MiSpiAnalyzerSettings::LoadSettings( const char* settings )
{
    SimpleArchive text_archive;
    text_archive.SetString( settings );

    const char* name_string; // the first thing in the archive is the name of the protocol analyzer that the data belongs to.
    text_archive >> &name_string;
    if( strcmp( name_string, "MiSpiAnalyzer" ) != 0 )
        AnalyzerHelpers::Assert( "MiSpiAnalyzer: Provided with a settings string that doesn't belong to us;" );

    text_archive >> mDataChannel;
    text_archive >> mClockChannel;
    text_archive >> *( U32* )&mShiftOrder;

    ClearChannels();
    AddChannel( mDataChannel, "DATA", mDataChannel != UNDEFINED_CHANNEL );
    AddChannel( mClockChannel, "CLOCK", mClockChannel != UNDEFINED_CHANNEL );

    UpdateInterfacesFromSettings();
}

const char* MiSpiAnalyzerSettings::SaveSettings()
{
    SimpleArchive text_archive;

    text_archive << "MiSpiAnalyzer";
    text_archive << mDataChannel;
    text_archive << mClockChannel;
    text_archive << mShiftOrder;

    return SetReturnString( text_archive.GetString() );
}

void MiSpiAnalyzerSettings::UpdateInterfacesFromSettings()
{
    mDataChannelInterface->SetChannel( mDataChannel );
    mClockChannelInterface->SetChannel( mClockChannel );
    mShiftOrderInterface->SetNumber( mShiftOrder );
}
