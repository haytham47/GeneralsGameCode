/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// FORK @feature 10/10/2026 Desync guard, see DesyncGuard.h.

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameNetwork/DesyncGuard.h"

#include "Common/crc.h"
#include "Common/FileSystem.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "GameClient/InGameUI.h"
#include "GameClient/MapUtil.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/ScriptEngine.h"
#include "GameNetwork/FileTransfer.h"
#include "GameNetwork/GameInfo.h"
#include "GameNetwork/NetworkInterface.h"
#include "Compression.h"

#include <algorithm>
#include <vector>

Int DesyncGuard::s_forceDesyncFrame = 0;
Int DesyncGuard::s_forceDesyncSlot = -1;

static DesyncSlotCRCs s_slots[MAX_SLOTS];
static UnsignedInt s_slotFrame[MAX_SLOTS];
static UnsignedInt s_fingerprint = 0;
static DesyncStateDumpFunc s_dumpFunc = nullptr;

struct DesyncCommandNote
{
	UnsignedInt frame;
	Int type;
	Int player;
};
enum { DESYNC_COMMAND_NOTES = 512 };
static DesyncCommandNote s_commands[DESYNC_COMMAND_NOTES];
static Int s_commandNext = 0;
static Int s_commandCount = 0;

static Bool s_collecting = FALSE;
static AsciiString s_folder;
static AsciiString s_localReportPath;
static UnsignedInt s_startMs = 0;

enum { DESYNC_COLLECT_TIMEOUT_MS = 30000, DESYNC_KEEP_FOLDERS = 20 };
static Bool s_received[MAX_SLOTS];
static UnsignedShort s_uploadCommandID = 0;
static Bool s_endTimerStarted = FALSE;

void DesyncGuard::reset()
{
	memset(s_slots, 0, sizeof(s_slots));
	memset(s_slotFrame, 0, sizeof(s_slotFrame));
	s_fingerprint = 0;
	s_commandNext = 0;
	s_commandCount = 0;
	s_collecting = FALSE;
	s_folder.clear();
	s_localReportPath.clear();
	s_startMs = 0;
	memset(s_received, 0, sizeof(s_received));
	s_uploadCommandID = 0;
	s_endTimerStarted = FALSE;
}

void DesyncGuard::setLocalFingerprint( UnsignedInt fingerprint ) { s_fingerprint = fingerprint; }
UnsignedInt DesyncGuard::getLocalFingerprint() { return s_fingerprint; }
void DesyncGuard::setStateDumpFunc( DesyncStateDumpFunc func ) { s_dumpFunc = func; }

void DesyncGuard::noteCommand( UnsignedInt frame, Int type, Int playerIndex )
{
	DesyncCommandNote &note = s_commands[s_commandNext];
	note.frame = frame;
	note.type = type;
	note.player = playerIndex;
	s_commandNext = (s_commandNext + 1) % DESYNC_COMMAND_NOTES;
	if (s_commandCount < DESYNC_COMMAND_NOTES)
		++s_commandCount;
}

void DesyncGuard::recordCRCMessage( Int slot, const GameMessage *msg )
{
	if (slot < 0 || slot >= MAX_SLOTS || msg == nullptr || msg->getArgumentCount() < 1)
		return;
	DesyncSlotCRCs &entry = s_slots[slot];
	entry.present = TRUE;
	entry.main = (UnsignedInt)msg->getArgument(0)->integer;
	entry.hasSections = (msg->getArgumentCount() >= 2 + DESYNC_SECTION_COUNT + 1);
	if (entry.hasSections)
	{
		for (Int s = 0; s < DESYNC_SECTION_COUNT; ++s)
			entry.sections[s] = (UnsignedInt)msg->getArgument(2 + s)->integer;
		entry.fingerprint = (UnsignedInt)msg->getArgument(2 + DESYNC_SECTION_COUNT)->integer;
	}
	s_slotFrame[slot] = TheGameLogic->getFrame();
}

static AsciiString makeTimeStamp()
{
	SYSTEMTIME st;
	GetLocalTime( &st );
	AsciiString stamp;
	stamp.format( "%04d%02d%02d_%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond );
	return stamp;
}

static AsciiString desyncRootDir()
{
	AsciiString root = TheGlobalData->getPath_UserData();
	root.concat( "Desync\\" );
	return root;
}

/// Creates the report folder once per game (host: the collection folder, clients: a "_sent" copy).
static const AsciiString &ensureFolder()
{
	if ( s_folder.isEmpty() )
	{
		const Bool amHost = TheGameInfo && TheGameInfo->amIHost();
		AsciiString root = desyncRootDir();
		TheFileSystem->createDirectory( root );
		s_folder.format( "%s%s%s\\", root.str(), makeTimeStamp().str(), amHost ? "" : "_sent" );
		TheFileSystem->createDirectory( s_folder );
	}
	return s_folder;
}

static AsciiString slotName( Int slot )
{
	AsciiString name;
	const GameSlot *gameSlot = TheGameInfo ? TheGameInfo->getConstSlot( slot ) : nullptr;
	if ( gameSlot )
		name.translate( gameSlot->getName() );
	return name;
}

static AsciiString localLeafName()
{
	char safe[48];
	const Int slot = TheGameInfo ? TheGameInfo->getLocalSlotNum() : -1;
	SanitizeDesyncFileName( slotName( slot ).str(), safe, sizeof(safe) );
	AsciiString leaf;
	leaf.format( "%d_%s", slot, safe );
	return leaf;
}

static void writeSlotLine( FILE *fp, Int slot, const DesyncSlotCRCs &s )
{
	fprintf( fp, "S slot=%d name=%s present=%d main=%08X", slot, slotName( slot ).str(), s.present ? 1 : 0, s.main );
	for ( Int section = 0; section < DESYNC_SECTION_COUNT; ++section )
		fprintf( fp, " %s=%08X", GetDesyncSectionName( section ), s.hasSections ? s.sections[section] : 0 );
	fprintf( fp, " fp=%08X\r\n", s.hasSections ? s.fingerprint : 0 );
}

static AsciiString sectionList( UnsignedInt bits )
{
	AsciiString list;
	for ( Int section = 0; section < DESYNC_SECTION_COUNT; ++section )
	{
		if ( ( bits & ( 1u << section ) ) == 0 )
			continue;
		if ( list.isNotEmpty() )
			list.concat( "," );
		list.concat( GetDesyncSectionName( section ) );
	}
	return list.isEmpty() ? AsciiString( "none" ) : list;
}

static void uploadLocalReport()
{
	FILE *fp = fopen( s_localReportPath.str(), "rb" );
	if ( fp == nullptr )
		return;
	fseek( fp, 0, SEEK_END );
	Int len = (Int)ftell( fp );
	fseek( fp, 0, SEEK_SET );
	if ( len > DESYNC_REPORT_MAX_BYTES )
		len = DESYNC_REPORT_MAX_BYTES; // the head of the report (header, CRCs, verdict, players) matters most
	if ( len <= 0 ) { fclose( fp ); return; }
	char *raw = NEW char[len];
	const Int got = (Int)fread( raw, 1, len, fp );
	fclose( fp );

	const CompressionType type = CompressionManager::getPreferredCompression();
	const Int maxPacked = CompressionManager::getMaxCompressedSize( got, type );
	char *packed = NEW char[maxPacked];
	const Int packedLen = CompressionManager::compressData( type, raw, got, packed, maxPacked );

	AsciiString leaf = localLeafName();
	leaf.concat( ".txt" );
	if ( packedLen > 0 )
		s_uploadCommandID = TheNetwork->sendDesyncReport( leaf, (const UnsignedByte *)packed, packedLen, 1 << 0 );
	else
		s_uploadCommandID = TheNetwork->sendDesyncReport( leaf, (const UnsignedByte *)raw, got, 1 << 0 );
	delete[] packed;
	delete[] raw;
}

Bool DesyncGuard::onMismatchDetected()
{
	if ( s_collecting || TheGameInfo == nullptr || TheNetwork == nullptr )
		return FALSE;
	s_collecting = TRUE;
	s_startMs = timeGetTime();

	const UnsignedInt frame = TheGameLogic->getFrame();
	DesyncSlotCRCs current[MAX_SLOTS];
	Int i;
	for ( i = 0; i < MAX_SLOTS; ++i )
	{
		current[i] = s_slots[i];
		current[i].present = s_slots[i].present && s_slotFrame[i] == frame && TheNetwork->isPlayerConnected( i );
	}
	const Int reference = FindDesyncReferenceSlot( current, MAX_SLOTS, 0 );

	const AsciiString &folder = ensureFolder();
	const AsciiString leaf = localLeafName();
	s_localReportPath.format( "%s%s.txt", folder.str(), leaf.str() );
	FILE *fp = fopen( s_localReportPath.str(), "wb" );
	if ( fp )
	{
		fprintf( fp, "# DesyncGuard report v1\r\n[header]\r\n" );
		fprintf( fp, "player=%s\r\nslot=%d\r\nframe=%u\r\nhost=%d\r\n", slotName( TheGameInfo->getLocalSlotNum() ).str(),
			TheGameInfo->getLocalSlotNum(), frame, TheGameInfo->amIHost() ? 1 : 0 );
		fprintf( fp, "exeCRC=%08X iniCRC=%08X fingerprint=%08X\r\nmap=%s\r\n", TheGlobalData->m_exeCRC,
			TheGlobalData->m_iniCRC, s_fingerprint, TheGameInfo->getMap().str() );
		fprintf( fp, "[crc]\r\n" );
		for ( i = 0; i < MAX_SLOTS; ++i )
		{
			if ( TheGameInfo->getConstSlot( i ) && TheGameInfo->getConstSlot( i )->isHuman() )
				writeSlotLine( fp, i, current[i] );
		}
		fprintf( fp, "[verdict]\r\nreference=%d\r\n", reference );
		for ( i = 0; i < MAX_SLOTS; ++i )
		{
			if ( reference < 0 || !current[i].present || current[i].main == current[reference].main )
				continue;
			const UnsignedInt bits = DiffDesyncSections( current[i], current[reference] );
			fprintf( fp, "outlier slot=%d name=%s sections=%s data=%s\r\n", i, slotName( i ).str(),
				sectionList( bits ).str(), ( bits & DESYNC_DIFF_DATA ) ? "DIFFERENT" : "same" );
		}
		if ( s_dumpFunc )
		{
			AsciiString deepPath;
			deepPath.format( "%s%s_deep.bin", folder.str(), leaf.str() );
			s_dumpFunc( fp, deepPath );
		}
		fprintf( fp, "[commands]\r\n" );
		for ( Int n = 0; n < s_commandCount; ++n )
		{
			const Int index = ( s_commandNext - s_commandCount + n + DESYNC_COMMAND_NOTES ) % DESYNC_COMMAND_NOTES;
			fprintf( fp, "C frame=%u type=%d player=%d\r\n", s_commands[index].frame, s_commands[index].type,
				s_commands[index].player );
		}
		fclose( fp );
	}

	// Tell the players who diverged (shown under the mismatch dialog). messageNoFormat: names may contain '%'.
	if ( TheInGameUI && reference >= 0 )
	{
		for ( i = 0; i < MAX_SLOTS; ++i )
		{
			if ( !current[i].present || current[i].main == current[reference].main )
				continue;
			const UnsignedInt bits = DiffDesyncSections( current[i], current[reference] );
			UnicodeString text;
			UnicodeString name = TheGameInfo->getConstSlot( i )->getName();
			if ( bits & DESYNC_DIFF_DATA )
				text.format( L"Out of sync: %ls has different game data.", name.str() );
			else
			{
				UnicodeString sections;
				sections.translate( sectionList( bits ) );
				text.format( L"Out of sync: %ls (%ls).", name.str(), sections.str() );
			}
			TheInGameUI->messageNoFormat( text );
		}
		TheInGameUI->messageNoFormat( UnicodeString( L"Saving mismatch diagnostics..." ) );
	}
	if ( !TheGameInfo->amIHost() )
		uploadLocalReport();
	return TRUE;
}

Bool DesyncGuard::onReportReceived( Int fromSlot, const AsciiString &leafName, const UnsignedByte *data, Int len )
{
	// Only the host of a running LAN game accepts reports, and only from a connected player. A report can arrive a
	// moment before the host's own detection of the same mismatch, so a seen mismatch is not required here.
	if ( TheGameInfo == nullptr || !TheGameInfo->amIHost() || TheNetwork == nullptr || !TheGameLogic->isInGame() )
		return FALSE;
	if ( fromSlot <= 0 || fromSlot >= MAX_SLOTS || !TheNetwork->isPlayerConnected( fromSlot ) || data == nullptr || !IsDesyncReportSizeOk( len ) )
		return FALSE;
	char safe[64];
	SanitizeDesyncFileName( leafName.str(), safe, sizeof(safe) );
	AsciiString path;
	path.format( "%sfrom%d_%s", ensureFolder().str(), fromSlot, safe );
	FILE *fp = fopen( path.str(), "wb" );
	if ( fp == nullptr )
		return FALSE;
	fwrite( data, 1, len, fp );
	fclose( fp );
	s_received[fromSlot] = TRUE;
	if ( TheInGameUI )
	{
		UnicodeString text;
		text.format( L"Received mismatch diagnostics from %ls.", TheGameInfo->getConstSlot( fromSlot )->getName().str() );
		TheInGameUI->messageNoFormat( text );
	}
	return TRUE;
}

void DesyncGuard::update()
{
	if ( !s_collecting || s_endTimerStarted || TheGameInfo == nullptr || TheNetwork == nullptr )
		return;
	Bool done = TRUE;
	if ( TheGameInfo->amIHost() )
	{
		for ( Int i = 1; i < MAX_SLOTS; ++i )
		{
			const GameSlot *slot = TheGameInfo->getConstSlot( i );
			if ( slot && slot->isHuman() && TheNetwork->isPlayerConnected( i ) && !s_received[i] )
				done = FALSE;
		}
	}
	else
	{
		done = ( s_uploadCommandID == 0 ) || TheNetwork->isFileTransferAcked( 0, s_uploadCommandID );
	}
	const Bool timedOut = ( timeGetTime() - s_startMs ) > (UnsignedInt)DESYNC_COLLECT_TIMEOUT_MS;
	if ( !done && !timedOut )
		return;
	s_endTimerStarted = TRUE;
	if ( TheInGameUI )
	{
		UnicodeString text;
		if ( TheGameInfo->amIHost() )
			text.format( L"Mismatch diagnostics saved in Documents\\...\\Desync%ls.", done ? L"" : L" (some players did not answer)" );
		else
			text = done ? UnicodeString( L"Mismatch diagnostics sent to the host." ) : UnicodeString( L"Could not send mismatch diagnostics to the host." );
		TheInGameUI->messageNoFormat( text );
	}
	TheScriptEngine->startEndGameTimer();
}

static void deleteFolder( const AsciiString &folder )
{
	AsciiString pattern;
	pattern.format( "%s*", folder.str() );
	WIN32_FIND_DATA data;
	HANDLE find = FindFirstFile( pattern.str(), &data );
	if ( find != INVALID_HANDLE_VALUE )
	{
		do
		{
			if ( ( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) == 0 )
			{
				AsciiString file;
				file.format( "%s%s", folder.str(), data.cFileName );
				DeleteFile( file.str() );
			}
		} while ( FindNextFile( find, &data ) );
		FindClose( find );
	}
	AsciiString noSlash = folder;
	noSlash.removeLastChar();
	RemoveDirectory( noSlash.str() );
}

static bool lessAscii( const AsciiString &a, const AsciiString &b )
{
	return strcmp( a.str(), b.str() ) < 0;
}

/// Folder names start with YYYYMMDD_HHMMSS, so name order is age order.
static void pruneOldFolders()
{
	const AsciiString root = desyncRootDir();
	AsciiString pattern;
	pattern.format( "%s*", root.str() );
	std::vector<AsciiString> folders;
	WIN32_FIND_DATA data;
	HANDLE find = FindFirstFile( pattern.str(), &data );
	if ( find == INVALID_HANDLE_VALUE )
		return;
	do
	{
		if ( ( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) && data.cFileName[0] != '.' )
			folders.push_back( AsciiString( data.cFileName ) );
	} while ( FindNextFile( find, &data ) );
	FindClose( find );
	std::sort( folders.begin(), folders.end(), lessAscii );
	for ( Int i = 0; i + DESYNC_KEEP_FOLDERS < (Int)folders.size(); ++i )
	{
		AsciiString path;
		path.format( "%s%s\\", root.str(), folders[i].str() );
		deleteFolder( path );
	}
}

void DesyncGuard::onReplayClosed( const AsciiString &replayPath )
{
	if ( s_folder.isEmpty() )
		return;
	AsciiString dest;
	dest.format( "%sreplay.rep", s_folder.str() );
	CopyFile( replayPath.str(), dest.str(), FALSE );
	pruneOldFolders();
}

UnsignedInt ComputeLocalDataFingerprint( const GameInfo *game )
{
	if (game == nullptr)
		return 0;
	UnsignedInt values[5];
	values[0] = TheGlobalData->m_exeCRC;
	values[1] = TheGlobalData->m_iniCRC;
	const MapMetaData *mapData = TheMapCache ? TheMapCache->findMap(game->getMap()) : nullptr;
	values[2] = mapData ? mapData->m_CRC : 0;
	values[3] = ComputeMapAuxCRC(game->getMap());
	values[4] = ComputeGameOptionsCRC(game);
	CRC crc;
	crc.clear();
	crc.computeCRC(values, sizeof(values));
	return crc.get();
}
