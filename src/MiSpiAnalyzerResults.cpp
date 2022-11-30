#include "MiSpiAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "MiSpiAnalyzer.h"
#include "MiSpiAnalyzerSettings.h"
#include <iostream>
#include <sstream>
#include <vector>

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
    } else if (frame.mType == MiSpiSync) {
        AddResultString( "Y" );
        AddResultString( "Sync" );
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

    ss << "Direction,Repetitions,";

    if (mSettings->mShiftOrder == AnalyzerEnums::MsbFirst) {
        ss << "Data (MSB First)";
    } else {
        ss << "Data (LSB First)";
    }

    for (int i = 2; i < 27; i++) {
        ss << "," << i;
    }
    ss << std::endl;

    AnalyzerHelpers::AppendToFile( ( U8* )ss.str().c_str(), ss.str().length(), f );
    ss.str( std::string() ); 

    MiSpiDirection direction = MiSpiDirUnknown;

    std::vector<U64> mosi_packet;
    std::vector<U64> miso_packet;
    std::vector<U64> new_packet;

    U8 mosi_reps = 1;
    U8 miso_reps = 1;

    U64 num_frames = GetNumFrames();
    for( U32 i = 0; i < num_frames; i++ )
    {
        Frame frame = GetFrame( i );
        // Switch on frame type,
        // if it's a direction frame, set the direction variable, and ...
        //    ... if we also already had a direction, commit packet to deduplicator for that direction
        if ( frame.mType == MiSpiStartMosi ) {
            if ( direction == MiSpiDirMiso )    {
                SubmitMisoPacket(f, display_base);
            } else if (direction == MiSpiDirMosi ) {
                SubmitMosiPacket(f, display_base);
            }
            direction = MiSpiDirMosi;
        } else if ( frame.mType == MiSpiStartMiso ) {
            if ( direction == MiSpiDirMosi ) {
                SubmitMosiPacket(f, display_base);
            } else if (direction == MiSpiDirMiso) {
                SubmitMisoPacket(f, display_base);
            }
            direction = MiSpiDirMiso;
        } else if ( frame.mType == MiSpiData ) {
        // if it's a data frame, and we don't have a direction yet, discard it
        // if it's a data frame, and we DO have a direction, commit frame to packet
            if ( direction != MiSpiDirUnknown ) {
                SubmitFrame(frame);
            }
        // on any other frame, reset the state machine
        } else {
            // and close whatever packet we were working on, if there was one
            if (direction == MiSpiDirMosi) {
                SubmitMosiPacket(f, display_base);
                CloseMosiPacket(f, display_base);
            } else if (direction == MiSpiDirMiso) {
                SubmitMisoPacket(f, display_base);
                CloseMisoPacket(f, display_base);
            }

            //Record sync packets
            if ( frame.mType == MiSpiSync) {
                std::stringstream ss;
                ss << "Sync" << std::endl;
                AnalyzerHelpers::AppendToFile( ( U8* )ss.str().c_str(), ss.str().length(), f );
                ss.str( std::string() );     
            }

            direction = MiSpiDirUnknown;
        }

        if( UpdateExportProgressAndCheckForCancel( i, num_frames ) == true )
        {
            ClosePackets(f, display_base, direction);
            AnalyzerHelpers::EndFile( f );
            return;
        }
    }

    ClosePackets(f, display_base, direction);
    UpdateExportProgressAndCheckForCancel( num_frames, num_frames );
    AnalyzerHelpers::EndFile( f );
}

void MiSpiAnalyzerResults::SubmitFrame(Frame frame) {
    new_packet.push_back(frame.mData1);
}

void MiSpiAnalyzerResults::CloseMisoPacket(void *f, DisplayBase display_base) {
    // Print the direction, rep count, packet
    std::stringstream ss; 
    char rep_str[ 128 ] = "";
    AnalyzerHelpers::GetNumberString( miso_reps, DisplayBase::Decimal, 8, rep_str, 128 );
    ss << "MISO," << rep_str;
    for (int i = 0; i < miso_packet.size(); i++) {
        char data_str[ 128 ] = "";
        AnalyzerHelpers::GetNumberString( miso_packet[i], display_base, 8, data_str, 128 );
        ss << "," << data_str;
    }
    ss << std::endl;
    AnalyzerHelpers::AppendToFile( ( U8* )ss.str().c_str(), ss.str().length(), f );
    ss.str( std::string() ); 

    miso_packet.resize(0);
}

void MiSpiAnalyzerResults::CloseMosiPacket(void *f, DisplayBase display_base) {
    // Print the direction, rep count, packet
    std::stringstream ss; 
    char rep_str[ 128 ] = "";
    AnalyzerHelpers::GetNumberString( mosi_reps, DisplayBase::Decimal, 8, rep_str, 128 );
    ss << "MOSI," << rep_str;
    for (int i = 0; i < mosi_packet.size(); i++) {
        char data_str[ 128 ] = "";
        AnalyzerHelpers::GetNumberString( mosi_packet[i], display_base, 8, data_str, 128 );
        ss << "," << data_str;
    }
    ss << std::endl;
    AnalyzerHelpers::AppendToFile( ( U8* )ss.str().c_str(), ss.str().length(), f );
    ss.str( std::string() ); 

    mosi_packet.resize(0);
}

// Print the last packets of the capture
void MiSpiAnalyzerResults::ClosePackets(void *f, DisplayBase display_base, int direction) {
    // Print them in the order we received them
    if (direction == MiSpiDirMosi) {
        CloseMosiPacket(f, display_base);
        CloseMisoPacket(f, display_base);
    } else {
        CloseMisoPacket(f, display_base);
        CloseMosiPacket(f, display_base);
    }
}

void MiSpiAnalyzerResults::SubmitMisoPacket(void *f, DisplayBase display_base) {
    if (new_packet == miso_packet) {
        // Nothing new here
        miso_reps++;
    } else {
        if (miso_packet.size() > 0) {
            CloseMisoPacket(f, display_base);
        }
        // The new packet becomes the reference
        miso_packet.resize(new_packet.size());
        miso_reps = 1;
        for (int i = 0; i < new_packet.size(); i++) {
            miso_packet[i] = new_packet[i];
        }
    }
    new_packet.resize(0);
}

void MiSpiAnalyzerResults::SubmitMosiPacket(void *f, DisplayBase display_base) {
    if (new_packet == mosi_packet) {
        // Nothing new here
        mosi_reps++;
    } else {
        if (mosi_packet.size() > 0) {
            CloseMosiPacket(f, display_base);
        }
        // The new packet becomes the reference
        mosi_packet.resize(new_packet.size());
        mosi_reps = 1;
        for (int i = 0; i < new_packet.size(); i++) {
            mosi_packet[i] = new_packet[i];
        }
    }
    new_packet.resize(0);
}

void MiSpiAnalyzerResults::GenerateFrameTabularText( U64 frame_index, DisplayBase display_base )
{
    ClearTabularText();
    Frame frame = GetFrame( frame_index );

    char data_str[ 128 ];

    std::stringstream ss;

    if( ( frame.mFlags & SPI_ERROR_FLAG ) == 0 )
    {
        AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, data_str, 128 );

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
