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

// FORK @feature 10/10/2026 Fingerprint of a map folder's logic files (map.ini, map.str, solo.ini) for the LAN lobby.

#pragma once

#include "Lib/BaseType.h"
#include "Common/crc.h"
#include <string.h>

/// Adds one map folder file to a fingerprint. A missing file still adds its tag and a 0 flag, so "no map.ini" and
/// "empty map.ini" give different fingerprints. Call in a fixed file order.
inline void MapFingerprintAddFile(CRC &crc, const char *tag, const void *data, Int len, Bool present)
{
	crc.computeCRC(tag, (Int)strlen(tag));
	const UnsignedByte flag = present ? 1 : 0;
	crc.computeCRC(&flag, 1);
	if (present && data != nullptr && len > 0)
		crc.computeCRC(data, len);
}
