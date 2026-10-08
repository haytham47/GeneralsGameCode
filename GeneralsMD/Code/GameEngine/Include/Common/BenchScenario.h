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

// FORK @feature 08/10/2026 Benchmark scenario file (plain key = value text, read by the -bench mode).

#pragma once

#include "Common/AsciiString.h"

#include <vector>

struct BenchWaveEntry
{
	AsciiString templateName;
	Int count;
};

struct BenchFactionSetup
{
	AsciiString faction;										///< e.g. FactionAmerica
	std::vector<BenchWaveEntry> wave;				///< units spawned per wave
	std::vector<AsciiString> structures;		///< structures placed at the base and along lanes
};

struct BenchScenario
{
	AsciiString name;
	AsciiString map;
	Int players;
	UnsignedInt seed;
	UnsignedInt frames;
	UnsignedInt waveEvery;
	Int armyCap;
	UnsignedInt orderEvery;
	Int groupSize;
	UnsignedInt structureEvery;
	Int maxLaneStructures;
	Int startCash;
	UnsignedInt crcEvery;
	UnsignedInt loadCap;					///< per-player build cap passed in the game options (0 = off)
	AsciiString capTestUnit;			///< capTest = <unit>:<factory>: checks the cap refuses that unit at the factory
	AsciiString capTestFactory;
	std::vector<AsciiString> slotFactions;
	std::vector<BenchFactionSetup> factions;
	std::vector<AsciiString> chokeWaypoints;

	BenchScenario();

	/// Parse the file. Returns FALSE and fills error when the file is missing or invalid.
	Bool parse(const char *path, AsciiString &error);

	const BenchFactionSetup *findFaction(const AsciiString &faction) const;
};
