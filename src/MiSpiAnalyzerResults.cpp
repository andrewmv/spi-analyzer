#include "MiSpiAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "MiSpiAnalyzer.h"
#include "MiSpiAnalyzerSettings.h"
#include <iostream>
#include <sstream>

#pragma warning( disable : 4996 ) // warning C4996: 'sprintf': This function or variable may be unsafe. Consider using sprintf_s instead.

MiSpiAnalyzerResults::MiSpiAnalyzerResults( MiSpiAnalyzer* analyzer, MiSpiAnalyzerSettings* settings )
    : AnalyzerResults(), mSettings( settings ), mAnalyzer( analyzer )
{
}

MiSpiAnalyzerResults::~MiSpiAnalyzerResults()
{
}

void MiSpiAnalyzerResults::GenerateBubbleText( U64 frame_index, Channel& channel,
                                             DisplayBase display_base ) // unrefereced vars commented out to remove warnings.
{
    ClearResultStrings();
    Frame frame = GetFrame( frame_index );

    if (frame.mType == MiSpiStartMosi) {
        AddResultString( "SO" );
        AddResultString( "MOSI" );
        AddResultString( "MOSI S" );
        AddResultString( "MOSI Start" );
    } else if(frame.mType == MiSpiStartMiso) {
        AddResultString( "SI" );
        AddResultString( "MISO" );
        AddResultString( "MISO S" );
        AddResultString( "MISO Start" );
    } else if (frame.mType == MiSpiData) {
        char number_str[128];
        AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );
        AddResultString( number_str );
    } else if (frame.mType == MiSpiError) {
        AddResultString( "Invalid" );
    }
}

void MiSpiAnalyzerResults::GenerateExportFile( const char* file, DisplayBase display_base, U32 /*export_type_user_id*/ )
{
    // export_type_user_id is only important if we have more than one export type.

    std::stringstream ss;
    void* f = AnalyzerHelpers::StartFile( file );

    U64 trigger_sample = mAnalyzer->GetTriggerSample();
    U32 sample_rate = mAnalyzer->GetSampleRate();
    U64 packet_count = GetNumPackets();

    ss << "Time [s],Packet ID,Total Packets,DATA" << std::endl;

    U64 num_frames = GetNumFrames();
    for( U32 i = 0; i < num_frames; i++ )
    {
        Frame frame = GetFrame( i );

        char time_str[ 128 ];
        AnalyzerHelpers::GetTimeString( frame.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, 128 );

        char data_str[ 128 ] = "";
        AnalyzerHelpers::GetNumberString( frame.mData1, display_base, mSettings->mBitsPerTransfer, data_str, 128 );

        U64 packet_id = GetPacketContainingFrameSequential( i );
        if( packet_id != INVALID_RESULT_INDEX )
            ss << time_str << "," << packet_id << "/" << packet_count << "," << data_str << std::endl;
        else
            ss << time_str << ",," << data_str << std::endl; // it's ok for a frame not to be included in a packet.

        AnalyzerHelpers::AppendToFile( ( U8* )ss.str().c_str(), ss.str().length(), f );
        ss.str( std::string() );

        if( UpdateExportProgressAndCheckForCancel( i, num_frames ) == true )
        {
            AnalyzerHelpers::EndFile( f );
            return;
        }
    }


    UpdateExportProgressAndCheckForCancel( num_frames, num_frames );
    AnalyzerHelpers::EndFile( f );
}

void MiSpiAnalyzerResults::GenerateFrameTabularText( U64 frame_index, DisplayBase display_base )
{
    ClearTabularText();
    Frame frame = GetFrame( frame_index );

    char data_str[ 128 ];

    std::stringstream ss;

    if( ( frame.mFlags & SPI_ERROR_FLAG ) == 0 )
    {
        AnalyzerHelpers::GetNumberString( frame.mData1, display_base, mSettings->mBitsPerTransfer, data_str, 128 );

        ss << "DATA: " << data_str;
    }
    else
    {
        ss << "The initial (idle) state of the CLK line does not match the settings.";
    }

    AddTabularText( ss.str().c_str() );
}

void MiSpiAnalyzerResults::GeneratePacketTabularText( U64 /*packet_id*/,
                                                    DisplayBase /*display_base*/ ) // unrefereced vars commented out to remove warnings.
{
    ClearResultStrings();
    AddResultString( "not supported" );
}

void
    MiSpiAnalyzerResults::GenerateTransactionTabularText( U64 /*transaction_id*/,
                                                        DisplayBase /*display_base*/ ) // unrefereced vars commented out to remove warnings.
{
    ClearResultStrings();
    AddResultString( "not supported" );
}
