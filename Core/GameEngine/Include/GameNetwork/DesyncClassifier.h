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

// FORK @feature 10/10/2026 Classifies a CRC mismatch (who diverged, in which part of the logic state) and names the
// report files of the desync guard. Pure functions, unit tested.

#pragma once

#include "Lib/BaseType.h"
#include <string.h>

enum DesyncSection
{
	DESYNC_SECTION_OBJECTS = 0,
	DESYNC_SECTION_RANDOM,
	DESYNC_SECTION_PARTITION,
	DESYNC_SECTION_PLAYERS,
	DESYNC_SECTION_AI,
	DESYNC_SECTION_COUNT
};

/// Set by DiffDesyncSections() when the game data fingerprint differs.
static const UnsignedInt DESYNC_DIFF_DATA = 0x80000000u;
/// Largest report accepted from another player.
static const Int DESYNC_REPORT_MAX_BYTES = 8 * 1024 * 1024;

struct DesyncSlotCRCs
{
	Bool present;                                  ///< sent a CRC on the mismatch frame
	Bool hasSections;                              ///< the message carried section CRCs (same build)
	UnsignedInt main;                              ///< the logic CRC compared by the game
	UnsignedInt sections[DESYNC_SECTION_COUNT];    ///< CRC of each part of the logic state
	UnsignedInt fingerprint;                       ///< exe, INI, map, map folder files and options of that PC
};

inline const char *GetDesyncSectionName(Int section)
{
	static const char *const names[DESYNC_SECTION_COUNT] = { "Objects", "Random", "Partition", "Players", "AI" };
	if (section < 0 || section >= DESYNC_SECTION_COUNT)
		return "?";
	return names[section];
}

/// A slot of the largest group of equal main CRCs. Ties go to the group that holds hostSlot, then to the lowest slot.
/// Returns -1 when no slot is present.
inline Int FindDesyncReferenceSlot(const DesyncSlotCRCs *slots, Int count, Int hostSlot)
{
	Int best = -1;
	Int bestVotes = 0;
	Bool bestHasHost = FALSE;
	for (Int i = 0; i < count; ++i)
	{
		if (!slots[i].present)
			continue;
		Int votes = 0;
		Bool hasHost = FALSE;
		for (Int j = 0; j < count; ++j)
		{
			if (slots[j].present && slots[j].main == slots[i].main)
			{
				++votes;
				if (j == hostSlot)
					hasHost = TRUE;
			}
		}
		if (best < 0 || votes > bestVotes || (votes == bestVotes && hasHost && !bestHasHost))
		{
			best = i;
			bestVotes = votes;
			bestHasHost = hasHost;
		}
	}
	return best;
}

/// Bit (1 << section) for every section that differs from the reference, plus DESYNC_DIFF_DATA when the data
/// fingerprint differs. 0 when either side has no section CRCs.
inline UnsignedInt DiffDesyncSections(const DesyncSlotCRCs &slot, const DesyncSlotCRCs &reference)
{
	UnsignedInt bits = 0;
	if (!slot.hasSections || !reference.hasSections)
		return 0;
	for (Int s = 0; s < DESYNC_SECTION_COUNT; ++s)
	{
		if (slot.sections[s] != reference.sections[s])
			bits |= (1u << s);
	}
	if (slot.fingerprint != reference.fingerprint)
		bits |= DESYNC_DIFF_DATA;
	return bits;
}

/// Makes a safe file name: keeps A-Z a-z 0-9 _ -, keeps '.' once a letter or digit was written, replaces the rest
/// with '_'. Truncates to outSize-1 characters. An empty result becomes "unnamed".
inline void SanitizeDesyncFileName(const char *in, char *out, Int outSize)
{
	Int n = 0;
	Bool sawAlnum = FALSE;
	for (const char *p = in; p && *p && n < outSize - 1; ++p)
	{
		const char c = *p;
		const Bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
		if (alnum || c == '_' || c == '-')
			out[n++] = c;
		else if (c == '.' && sawAlnum)
			out[n++] = c;
		else
			out[n++] = '_';
		if (alnum)
			sawAlnum = TRUE;
	}
	out[n] = '\0';
	if (n == 0 && outSize > 7)
		strcpy(out, "unnamed");
}

inline Bool IsDesyncReportSizeOk(Int len)
{
	return len > 0 && len <= DESYNC_REPORT_MAX_BYTES;
}
