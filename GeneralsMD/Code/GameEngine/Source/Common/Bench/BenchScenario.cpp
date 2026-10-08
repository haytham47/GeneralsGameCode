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

#include "PreRTS.h"

#include "Common/BenchMetrics.h"
#include "Common/BenchScenario.h"
#include "GameNetwork/NetworkDefs.h"

namespace
{
	AsciiString trimmed(const char *s)
	{
		AsciiString a(s);
		a.trim();
		return a;
	}

	void splitList(const AsciiString &value, std::vector<AsciiString> &out)
	{
		out.clear();
		AsciiString rest = value;
		AsciiString token;
		while (rest.nextToken(&token, ","))
		{
			token.trim();
			if (!token.isEmpty())
				out.push_back(token);
		}
	}
}

BenchScenario::BenchScenario() :
	players(0),
	seed(1),
	frames(9000),
	waveEvery(150),
	armyCap(200),
	orderEvery(300),
	groupSize(40),
	structureEvery(600),
	maxLaneStructures(6),
	startCash(100000),
	crcEvery(300),
	loadCap(0)
{
}

const BenchFactionSetup *BenchScenario::findFaction(const AsciiString &faction) const
{
	for (size_t i = 0; i < factions.size(); ++i)
	{
		if (factions[i].faction.compareNoCase(faction) == 0)
			return &factions[i];
	}
	return nullptr;
}

Bool BenchScenario::parse(const char *path, AsciiString &error)
{
	FILE *f = fopen(path, "r");
	if (!f)
	{
		error.format("cannot open scenario file '%s'", path);
		return FALSE;
	}

	name = path;
	BenchFactionSetup *section = nullptr;
	char line[2048];
	Int lineNo = 0;
	while (fgets(line, sizeof(line), f))
	{
		++lineNo;
		char *comment = strchr(line, ';');
		if (comment)
			*comment = '\0';
		AsciiString text = trimmed(line);
		if (text.isEmpty())
			continue;

		if (text.getCharAt(0) == '[')
		{
			if (text.getLength() < 3 || text.getCharAt(text.getLength() - 1) != ']')
			{
				error.format("line %d: expected [FactionName]", lineNo);
				fclose(f);
				return FALSE;
			}
			AsciiString faction = text;
			faction.removeLastChar();
			AsciiString inner(faction.str() + 1);
			inner.trim();
			BenchFactionSetup setup;
			setup.faction = inner;
			factions.push_back(setup);
			section = &factions.back();
			continue;
		}

		const char *eq = strchr(text.str(), '=');
		if (!eq)
		{
			error.format("line %d: expected key = value", lineNo);
			fclose(f);
			return FALSE;
		}
		AsciiString key;
		key.set(text.str(), (Int)(eq - text.str()));
		key.trim();
		AsciiString value = trimmed(eq + 1);

		if (section)
		{
			if (key.compareNoCase("wave") == 0)
			{
				std::vector<AsciiString> items;
				splitList(value, items);
				for (size_t i = 0; i < items.size(); ++i)
				{
					const char *colon = strchr(items[i].str(), ':');
					BenchWaveEntry entry;
					if (colon)
					{
						entry.templateName.set(items[i].str(), (Int)(colon - items[i].str()));
						entry.templateName.trim();
						entry.count = atoi(colon + 1);
					}
					else
					{
						entry.templateName = items[i];
						entry.count = 1;
					}
					section->wave.push_back(entry);
				}
			}
			else if (key.compareNoCase("structures") == 0)
			{
				splitList(value, section->structures);
			}
			else
			{
				error.format("line %d: unknown faction key '%s'", lineNo, key.str());
				fclose(f);
				return FALSE;
			}
			continue;
		}

		if (key.compareNoCase("map") == 0) map = value;
		else if (key.compareNoCase("players") == 0) players = atoi(value.str());
		else if (key.compareNoCase("seed") == 0) seed = (UnsignedInt)strtoul(value.str(), nullptr, 10);
		else if (key.compareNoCase("frames") == 0) frames = (UnsignedInt)atoi(value.str());
		else if (key.compareNoCase("waveEvery") == 0) waveEvery = (UnsignedInt)atoi(value.str());
		else if (key.compareNoCase("armyCap") == 0) armyCap = atoi(value.str());
		else if (key.compareNoCase("orderEvery") == 0) orderEvery = (UnsignedInt)atoi(value.str());
		else if (key.compareNoCase("groupSize") == 0) groupSize = atoi(value.str());
		else if (key.compareNoCase("structureEvery") == 0) structureEvery = (UnsignedInt)atoi(value.str());
		else if (key.compareNoCase("maxLaneStructures") == 0) maxLaneStructures = atoi(value.str());
		else if (key.compareNoCase("startCash") == 0) startCash = atoi(value.str());
		else if (key.compareNoCase("crcEvery") == 0) crcEvery = (UnsignedInt)atoi(value.str());
		else if (key.compareNoCase("factions") == 0) splitList(value, slotFactions);
		else if (key.compareNoCase("chokeWaypoints") == 0) splitList(value, chokeWaypoints);
		else if (key.compareNoCase("loadCap") == 0) loadCap = (UnsignedInt)atoi(value.str());
		else if (key.compareNoCase("capTest") == 0)
		{
			const char *colon = strchr(value.str(), ':');
			if (!colon)
			{
				error.format("line %d: capTest = <unit>:<factory>", lineNo);
				fclose(f);
				return FALSE;
			}
			capTestUnit.set(value.str(), (Int)(colon - value.str()));
			capTestUnit.trim();
			capTestFactory = colon + 1;
			capTestFactory.trim();
		}
		else
		{
			error.format("line %d: unknown key '%s'", lineNo, key.str());
			fclose(f);
			return FALSE;
		}
	}
	fclose(f);

	if (map.isEmpty())
	{
		error = "scenario has no map";
		return FALSE;
	}
	if (players < 2 || players > MAX_SLOTS)
	{
		error.format("players must be 2..%d (got %d)", MAX_SLOTS, players);
		return FALSE;
	}
	if (slotFactions.empty())
	{
		error = "scenario has no factions list";
		return FALSE;
	}
	for (size_t i = 0; i < slotFactions.size(); ++i)
	{
		if (!findFaction(slotFactions[i]))
		{
			error.format("faction '%s' has no [section]", slotFactions[i].str());
			return FALSE;
		}
	}
	// Negative numbers parsed into unsigned fields wrap to huge values; reject them.
	if (frames > 10000000u || waveEvery > 10000000u || orderEvery > 10000000u || structureEvery > 10000000u || crcEvery > 10000000u)
	{
		error = "a frame count is negative or too large";
		return FALSE;
	}
	if (frames < 30 || waveEvery == 0 || orderEvery == 0 || structureEvery == 0 || groupSize < 1 || crcEvery == 0)
	{
		error = "frames/waveEvery/orderEvery/structureEvery/groupSize/crcEvery out of range";
		return FALSE;
	}
	return TRUE;
}
