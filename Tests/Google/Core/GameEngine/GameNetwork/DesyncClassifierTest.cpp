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

// FORK @feature 10/10/2026 Tests the mismatch classification and report naming of the desync guard.

#include <gtest/gtest.h>

#include "GameNetwork/DesyncClassifier.h"

static DesyncSlotCRCs makeSlot(UnsignedInt mainCRC, UnsignedInt objects, UnsignedInt fingerprint)
{
	DesyncSlotCRCs s;
	memset(&s, 0, sizeof(s));
	s.present = TRUE;
	s.hasSections = TRUE;
	s.main = mainCRC;
	s.sections[DESYNC_SECTION_OBJECTS] = objects;
	s.fingerprint = fingerprint;
	return s;
}

TEST(DesyncClassifier, MajorityIsReference)
{
	DesyncSlotCRCs slots[5] = { makeSlot(1,1,7), makeSlot(1,1,7), makeSlot(2,9,7), makeSlot(1,1,7), makeSlot(1,1,7) };
	const Int ref = FindDesyncReferenceSlot(slots, 5, 0);
	EXPECT_EQ(slots[ref].main, 1u);
	EXPECT_EQ(DiffDesyncSections(slots[2], slots[ref]), 1u << DESYNC_SECTION_OBJECTS);
	EXPECT_EQ(DiffDesyncSections(slots[1], slots[ref]), 0u);
}

TEST(DesyncClassifier, TwoPlayersHostWins)
{
	DesyncSlotCRCs slots[2] = { makeSlot(5,5,7), makeSlot(6,6,7) };
	EXPECT_EQ(FindDesyncReferenceSlot(slots, 2, 0), 0);
}

TEST(DesyncClassifier, TieGoesToHostGroup)
{
	DesyncSlotCRCs slots[4] = { makeSlot(3,3,7), makeSlot(4,4,7), makeSlot(4,4,7), makeSlot(3,3,7) };
	EXPECT_EQ(slots[FindDesyncReferenceSlot(slots, 4, 0)].main, 3u);
}

TEST(DesyncClassifier, AbsentSlotsAreIgnored)
{
	DesyncSlotCRCs slots[3] = { makeSlot(8,8,7), makeSlot(9,9,7), makeSlot(9,9,7) };
	slots[1].present = FALSE;
	slots[2].present = FALSE;
	EXPECT_EQ(FindDesyncReferenceSlot(slots, 3, 1), 0);
	slots[0].present = FALSE;
	EXPECT_EQ(FindDesyncReferenceSlot(slots, 3, 1), -1);
}

TEST(DesyncClassifier, DataDifferenceIsFlagged)
{
	DesyncSlotCRCs a = makeSlot(1,1,7);
	DesyncSlotCRCs b = makeSlot(2,1,8);
	EXPECT_EQ(DiffDesyncSections(b, a), DESYNC_DIFF_DATA);
}

TEST(DesyncClassifier, NoSectionsNoDiff)
{
	DesyncSlotCRCs a = makeSlot(1,1,7);
	DesyncSlotCRCs b = makeSlot(2,2,8);
	b.hasSections = FALSE;
	EXPECT_EQ(DiffDesyncSections(b, a), 0u);
}

TEST(DesyncClassifier, SanitizesFileNames)
{
	char out[32];
	SanitizeDesyncFileName("..\\evil/name.txt", out, sizeof(out));
	EXPECT_STREQ(out, "___evil_name.txt");
	SanitizeDesyncFileName("Madara 2", out, sizeof(out));
	EXPECT_STREQ(out, "Madara_2");
	SanitizeDesyncFileName("", out, sizeof(out));
	EXPECT_STREQ(out, "unnamed");
	SanitizeDesyncFileName("abcdefghijklmnopqrstuvwxyz0123456789", out, 8);
	EXPECT_STREQ(out, "abcdefg");
}

TEST(DesyncClassifier, ReportSizeLimits)
{
	EXPECT_FALSE(IsDesyncReportSizeOk(0));
	EXPECT_TRUE(IsDesyncReportSizeOk(1000));
	EXPECT_TRUE(IsDesyncReportSizeOk(DESYNC_REPORT_MAX_BYTES));
	EXPECT_FALSE(IsDesyncReportSizeOk(DESYNC_REPORT_MAX_BYTES + 1));
}
