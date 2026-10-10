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
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "GameClient/MapUtil.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/FileTransfer.h"
#include "GameNetwork/GameInfo.h"

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

void DesyncGuard::reset()
{
	memset(s_slots, 0, sizeof(s_slots));
	memset(s_slotFrame, 0, sizeof(s_slotFrame));
	s_fingerprint = 0;
	s_commandNext = 0;
	s_commandCount = 0;
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

// Tasks 9 and 10 replace these bodies.
Bool DesyncGuard::onMismatchDetected() { return FALSE; }
void DesyncGuard::update() {}
Bool DesyncGuard::onReportReceived( Int, const AsciiString &, const UnsignedByte *, Int ) { return FALSE; }
void DesyncGuard::onReplayClosed( const AsciiString & ) {}

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
