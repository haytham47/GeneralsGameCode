/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

// FORK @feature 10/10/2026 Tests the map folder fingerprint used by the LAN lobby data check.

#include <gtest/gtest.h>

#include "GameNetwork/MapFingerprint.h"

static UnsignedInt fingerprintOf(const char *iniText, Bool hasIni)
{
	CRC crc;
	crc.clear();
	MapFingerprintAddFile(crc, "map.ini", iniText, hasIni ? (Int)strlen(iniText) : 0, hasIni);
	MapFingerprintAddFile(crc, "map.str", nullptr, 0, FALSE);
	return crc.get();
}

TEST(MapFingerprint, SameFilesSameValue)
{
	EXPECT_EQ(fingerprintOf("Weapon X\r\nEnd\r\n", TRUE), fingerprintOf("Weapon X\r\nEnd\r\n", TRUE));
}

TEST(MapFingerprint, ContentChangeChangesValue)
{
	EXPECT_NE(fingerprintOf("Damage = 10\r\n", TRUE), fingerprintOf("Damage = 11\r\n", TRUE));
}

TEST(MapFingerprint, MissingDiffersFromEmpty)
{
	EXPECT_NE(fingerprintOf("", TRUE), fingerprintOf("", FALSE));
}

TEST(MapFingerprint, FileOrderMatters)
{
	CRC a; a.clear();
	MapFingerprintAddFile(a, "map.ini", "x", 1, TRUE);
	MapFingerprintAddFile(a, "map.str", "y", 1, TRUE);
	CRC b; b.clear();
	MapFingerprintAddFile(b, "map.str", "y", 1, TRUE);
	MapFingerprintAddFile(b, "map.ini", "x", 1, TRUE);
	EXPECT_NE(a.get(), b.get());
}
