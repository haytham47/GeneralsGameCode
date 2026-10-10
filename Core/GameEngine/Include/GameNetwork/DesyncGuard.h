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

// FORK @feature 10/10/2026 Desync guard: collects every player's section CRCs, and on a CRC mismatch writes a report on
// every PC, sends the clients' reports to the host and delays the end of the game until the host has them.

#pragma once

#include "Common/AsciiString.h"
#include "GameNetwork/DesyncClassifier.h"
#include <stdio.h>

class GameInfo;
class GameMessage;

/// Portable file name prefix of a desync report sent over the game network (never a map path).
#define DESYNC_REPORT_PREFIX "desyncreport\\"

/// Writes the logic state dump of a report (players, objects) and optionally a deep CRC file. Set by the game.
typedef void (*DesyncStateDumpFunc)( FILE *fp, const AsciiString &deepDumpPath );

class DesyncGuard
{
public:
	static void reset();                                                    ///< new game
	static void setLocalFingerprint( UnsignedInt fingerprint );
	static UnsignedInt getLocalFingerprint();
	static void setStateDumpFunc( DesyncStateDumpFunc func );
	static void noteCommand( UnsignedInt frame, Int type, Int playerIndex ); ///< last commands for the report
	static void recordCRCMessage( Int slot, const GameMessage *msg );        ///< from GameLogic::onLogicCrc
	static Bool onMismatchDetected();                                        ///< TRUE: the guard ends the game later
	static void update();                                                    ///< every network update
	static Bool onReportReceived( Int fromSlot, const AsciiString &leafName, const UnsignedByte *data, Int len );
	static void onReplayClosed( const AsciiString &replayPath );

	static Int s_forceDesyncFrame;                                           ///< RTS_DEBUG test hook, 0 = off
	static Int s_forceDesyncSlot;
};

/// Fingerprint of the logic data this PC loaded: exe CRC, INI CRC, map CRC, map folder files, lobby options.
UnsignedInt ComputeLocalDataFingerprint( const GameInfo *game );
