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

// FORK @feature 08/10/2026 Tests the slowest-player selection used by the in-game network display.

#include <gtest/gtest.h>

#include "GameNetwork/SlowestPlayer.h"

TEST(SlowestPlayer, PicksLowestFps)
{
	const Int fps[4] = { 60, 24, 45, 30 };
	const Bool connected[4] = { TRUE, TRUE, TRUE, TRUE };
	EXPECT_EQ(FindSlowestSlot(fps, connected, 4), 1);
}

TEST(SlowestPlayer, TiesPickLowestSlot)
{
	const Int fps[4] = { 40, 30, 30, 50 };
	const Bool connected[4] = { TRUE, TRUE, TRUE, TRUE };
	EXPECT_EQ(FindSlowestSlot(fps, connected, 4), 1);
}

TEST(SlowestPlayer, IgnoresDisconnectedSlots)
{
	const Int fps[4] = { 40, 10, 30, 50 };
	const Bool connected[4] = { TRUE, FALSE, TRUE, TRUE };
	EXPECT_EQ(FindSlowestSlot(fps, connected, 4), 2);
}

TEST(SlowestPlayer, IgnoresUnknownFps)
{
	const Int fps[4] = { -1, 35, -1, 50 };
	const Bool connected[4] = { TRUE, TRUE, TRUE, TRUE };
	EXPECT_EQ(FindSlowestSlot(fps, connected, 4), 1);
}

TEST(SlowestPlayer, NoKnownPlayerReturnsMinusOne)
{
	const Int fps[3] = { -1, -1, 20 };
	const Bool connected[3] = { TRUE, TRUE, FALSE };
	EXPECT_EQ(FindSlowestSlot(fps, connected, 3), -1);
}
