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

// FORK @feature 08/10/2026 Drives the automated -bench mode: starts a skirmish from a scenario file,
// plays every slot like a human with real player messages at fixed logic frames, measures and quits.
// Everything the driver does is keyed on the logic frame and on deterministic object order, so a
// scenario produces the same game on every run of the same build (checked through crc.log).

#include "PreRTS.h"

#include "Common/BenchDriver.h"
#include "Common/BenchMetrics.h"
#include "Common/BenchScenario.h"
#include "Common/BuildAssistant.h"
#include "Common/GameEngine.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/RandomValue.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameClient/MapUtil.h"
#include "GameClient/View.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/VictoryConditions.h"
#include "GameNetwork/GameInfo.h"

#include <map>
#include <math.h>
#include <vector>

extern HWND ApplicationHWnd;

namespace
{
	BenchScenario s_scenario;
	Bool s_setupDone = FALSE;
	Bool s_finished = FALSE;
	Int s_exitCode = 0;
	UnsignedInt s_setupFrame = 0;
	UnsignedInt s_updatesBeforeSetup = 0;
	const UnsignedInt MAX_UPDATES_BEFORE_SETUP = 3000; ///< a game that does not start within this many updates fails the run

	Player *s_players[MAX_SLOTS];
	Coord3D s_start[MAX_SLOTS];
	Coord3D s_centre;
	Int s_laneStructures[MAX_SLOTS];
	std::vector<Coord3D> s_chokes;

	// Client-side bookkeeping for the freeze proxy: ObjectID -> logic frame since which the unit waits for a path.
	std::map<ObjectID, UnsignedInt> s_waitingSince;

	// FORK @feature 09/10/2026 capQueue trace: slot 0 load points, sampled once per second after the queue is filled.
	Bool s_capTraceActive = FALSE;
	ObjectID s_capTraceFactory = INVALID_ID;
	AsciiString s_capTrace;
	Int s_capTraceMin = 0x7fffffff;
	Int s_capTraceMax = 0;

	const UnsignedInt FREEZE_FRAMES = 90; ///< waiting longer than 3 s for a path counts as a frozen unit

	Bool isArmyUnit(const Object *obj)
	{
		if (obj->isEffectivelyDead())
			return FALSE;
		if (obj->isKindOf(KINDOF_STRUCTURE))
			return FALSE;
		return obj->isKindOf(KINDOF_INFANTRY) || obj->isKindOf(KINDOF_VEHICLE) || obj->isKindOf(KINDOF_AIRCRAFT);
	}

	Int slotOfPlayer(const Player *player)
	{
		for (Int i = 0; i < s_scenario.players; ++i)
		{
			if (s_players[i] == player)
				return i;
		}
		return -1;
	}

	void fail(const char *error)
	{
		printf("BENCH ERROR: %s\n", error);
		fflush(stdout);
		Bench::writeResults(1, error);
		s_exitCode = 1;
		s_finished = TRUE;
		TheGameEngine->setQuitting(TRUE);
	}

	void finish()
	{
		if (!TheGlobalData->m_headless)
			Bench::setInfoInt("window_was_minimized", (ApplicationHWnd && ::IsIconic(ApplicationHWnd)) ? 1 : 0);
		Bench::setInfoInt("setup_frame", (Int)s_setupFrame);
		const Bool ok = Bench::writeResults(0, "");
		printf("BENCH DONE: %u frames, results %s\n", TheGameLogic->getFrame(), ok ? "written" : "NOT written");
		fflush(stdout);
		s_exitCode = ok ? 0 : 1;
		s_finished = TRUE;
		TheGameEngine->setQuitting(TRUE);
	}

	void writeMapList(const char *file)
	{
		FILE *f = fopen(file, "w");
		if (!f)
		{
			printf("BENCH ERROR: cannot write %s\n", file);
			s_exitCode = 1;
			return;
		}
		fprintf(f, "map,numPlayers,official\n");
		for (MapCache::const_iterator it = TheMapCache->begin(); it != TheMapCache->end(); ++it)
		{
			const MapMetaData &md = it->second;
			if (!md.m_isMultiplayer)
				continue;
			fprintf(f, "\"%s\",%d,%d\n", it->first.str(), md.m_numPlayers, md.m_isOfficial ? 1 : 0);
		}
		fclose(f);
		printf("BENCH DONE: map list written to %s\n", file);
		fflush(stdout);
		s_exitCode = 0;
	}

	Int findPlayerTemplateIndex(const AsciiString &faction)
	{
		const PlayerTemplate *pt = ThePlayerTemplateStore->findPlayerTemplate(TheNameKeyGenerator->nameToKey(faction));
		if (!pt)
			return -1;
		for (Int j = 0; j < ThePlayerTemplateStore->getPlayerTemplateCount(); ++j)
		{
			if (ThePlayerTemplateStore->getNthPlayerTemplate(j) == pt)
				return j;
		}
		return -1;
	}

	Bool startGame(AsciiString &error)
	{
		AsciiString mapName = s_scenario.map;
		mapName.toLower();
		const MapMetaData *md = TheMapCache->findMap(mapName);
		if (!md)
		{
			error.format("map '%s' is not in the map cache", s_scenario.map.str());
			return FALSE;
		}
		if (!md->m_isMultiplayer || md->m_numPlayers < s_scenario.players)
		{
			error.format("map '%s' has %d start positions, scenario needs %d", s_scenario.map.str(), md->m_numPlayers, s_scenario.players);
			return FALSE;
		}

		if (!TheSkirmishGameInfo)
			TheSkirmishGameInfo = NEW SkirmishGameInfo;
		TheSkirmishGameInfo->init();
		TheSkirmishGameInfo->clearSlotList();
		TheSkirmishGameInfo->reset();
		TheSkirmishGameInfo->setLocalIP(TheSkirmishGameInfo->getSlot(0)->getIP());
		TheSkirmishGameInfo->enterGame();

		for (Int i = 0; i < MAX_SLOTS; ++i)
		{
			GameSlot slot;
			if (i < s_scenario.players)
			{
				const AsciiString &faction = s_scenario.slotFactions[i % s_scenario.slotFactions.size()];
				const Int templateIndex = findPlayerTemplateIndex(faction);
				if (templateIndex < 0)
				{
					error.format("unknown faction '%s'", faction.str());
					return FALSE;
				}
				if (i == 0)
				{
					UnicodeString name;
					name.format(L"Bench%d", i);
					slot.setState(SLOT_PLAYER, name);
				}
				else
				{
					// AI slots make the normal skirmish setup run; the AI brains are removed at setup.
					slot.setState(SLOT_EASY_AI);
				}
				slot.setColor(i);
				slot.setPlayerTemplate(templateIndex);
				slot.setStartPos(i);
				slot.setTeamNumber(-1);
			}
			else
			{
				slot.setState(SLOT_CLOSED);
			}
			TheSkirmishGameInfo->setSlot(i, slot);
		}

		Money cash;
		cash.deposit((UnsignedInt)s_scenario.startCash, FALSE, FALSE);
		TheSkirmishGameInfo->setStartingCash(cash);
		TheSkirmishGameInfo->setSuperweaponRestriction(0);
		TheSkirmishGameInfo->setLoadCap(s_scenario.loadCap);
		TheSkirmishGameInfo->setSeed((Int)s_scenario.seed);
		TheSkirmishGameInfo->setMap(mapName);
		TheSkirmishGameInfo->setMapCRC(md->m_CRC);
		TheSkirmishGameInfo->setMapSize(md->m_filesize);

		TheWritableGlobalData->m_mapName = mapName;
		TheWritableGlobalData->m_pendingFile = mapName;
		TheSkirmishGameInfo->startGame(0);
		InitRandom(s_scenario.seed);

		// Like replay playback, the new game message goes straight to the command list so that the
		// headless loop (which never propagates TheMessageStream) starts the game as well.
		GameMessage *msg = newInstance(GameMessage)(GameMessage::MSG_NEW_GAME);
		msg->appendIntegerArgument(GAME_SKIRMISH);
		msg->appendIntegerArgument(DIFFICULTY_NORMAL);
		msg->appendIntegerArgument(0);
		msg->appendIntegerArgument(1000);
		TheCommandList->appendMessage(msg);

		Bench::setInfo("map", mapName.str());
		Bench::setInfo("scenario", s_scenario.name.str());
		Bench::setInfoInt("players", s_scenario.players);
		Bench::setInfoInt("frames_target", (Int)s_scenario.frames);
		Bench::setInfoInt("army_cap", s_scenario.armyCap);
		Bench::setInfo("mode", TheGlobalData->m_headless ? "headless" : "window");
		return TRUE;
	}

	void deactivateAllScripts()
	{
		for (Int i = 0; i < TheSidesList->getNumSides(); ++i)
		{
			ScriptList *list = TheSidesList->getSideInfo(i)->getScriptList();
			if (!list)
				continue;
			for (Script *s = list->getScript(); s; s = s->getNext())
				s->setActive(FALSE);
			for (ScriptGroup *g = list->getScriptGroup(); g; g = g->getNext())
			{
				g->setActive(FALSE);
				for (Script *s = g->getScript(); s; s = s->getNext())
					s->setActive(FALSE);
			}
		}
	}

	Coord3D lerp(const Coord3D &a, const Coord3D &b, Real t)
	{
		Coord3D r;
		r.x = a.x + (b.x - a.x) * t;
		r.y = a.y + (b.y - a.y) * t;
		r.z = 0.0f;
		r.z = TheTerrainLogic->getGroundHeight(r.x, r.y);
		return r;
	}

	Object *spawnUnit(Player *player, const ThingTemplate *tmpl, const Coord3D &where)
	{
		Coord3D pos = where;
		pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
		Object *obj = TheThingFactory->newObject(tmpl, player->getDefaultTeam());
		if (!obj)
			return nullptr;
		obj->setOrientation(obj->getTemplate()->getPlacementViewAngle());
		obj->setPosition(&pos);
		for (BehaviorModule **m = obj->getBehaviorModules(); *m; ++m)
		{
			CreateModuleInterface *create = (*m)->getCreate();
			if (create)
				create->onBuildComplete();
		}
		Team *team = player->getDefaultTeam();
		if (team)
			team->setActive();
		TheAI->pathfinder()->addObjectToPathfindMap(obj);
		if (obj->getAIUpdateInterface() && !obj->isKindOf(KINDOF_IMMOBILE))
		{
			if (TheAI->pathfinder()->adjustDestination(obj, obj->getAIUpdateInterface()->getLocomotorSet(), &pos))
			{
				TheAI->pathfinder()->updateGoal(obj, &pos, LAYER_GROUND);
				obj->setPosition(&pos);
			}
		}
		player->onUnitCreated(nullptr, obj);
		return obj;
	}

	void placeStructure(Player *player, const AsciiString &name, const Coord3D &pos)
	{
		const ThingTemplate *tmpl = TheThingFactory->findTemplate(name);
		if (!tmpl)
			return;
		TheBuildAssistant->buildObjectNow(nullptr, tmpl, &pos, tmpl->getPlacementViewAngle(), player);
	}

	// Checks the per-player build cap on slot 0: below the cap the unit can be built, once the player's load reaches
	// the cap both Player::canBuildMoreOfType and BuildAssistant::canMakeUnit (used by the control bar, the production
	// queue, dozers and the AI) refuse it. Results go to summary.json (cap_test_ok = 1 when everything holds).
	void runCapTest()
	{
		// FORK @feature 09/10/2026 Load points of the listed templates (-1 = template not found).
		for (size_t i = 0; i < s_scenario.capPoints.size(); ++i)
		{
			const ThingTemplate *tmpl = TheThingFactory->findTemplate(s_scenario.capPoints[i]);
			AsciiString key;
			key.format("cap_points_%s", s_scenario.capPoints[i].str());
			Bench::setInfoInt(key.str(), tmpl ? tmpl->getLoadPoints() : -1);
		}

		if (s_scenario.capTestUnit.isEmpty())
			return;
		Player *player = s_players[0];
		const ThingTemplate *unit = TheThingFactory->findTemplate(s_scenario.capTestUnit);
		const ThingTemplate *factoryTmpl = TheThingFactory->findTemplate(s_scenario.capTestFactory);
		Object *factory = nullptr;
		for (Object *obj = TheGameLogic->getFirstObject(); obj && factoryTmpl; obj = obj->getNextObject())
		{
			if (obj->getControllingPlayer() == player && obj->getTemplate()->isEquivalentTo(factoryTmpl) && !obj->isEffectivelyDead())
			{
				factory = obj;
				break;
			}
		}
		if (!unit || !factory)
		{
			Bench::setInfo("cap_test_error", !unit ? "unit template not found" : "factory not found for slot 0");
			Bench::setInfoInt("cap_test_ok", 0);
			return;
		}

		const Int cap = (Int)TheGameLogic->getLoadCapForPlayer(player->getPlayerIndex()); // FORK @feature 09/10/2026 faction-weighted cap of this player

		// FORK @feature 09/10/2026 Queue variant: fill up to just under the cap, then fill the factory queue up to the cap
		// through the normal production path. postLogicUpdate traces the load every second while the queue drains.
		if (s_scenario.capQueue > 0)
		{
			const Coord3D anchor = lerp(s_start[0], s_centre, 0.18f);
			Int room = s_scenario.capQueue * unit->getLoadPoints();

			// optional: builders (one per structure) that start structures once the queue is full
			const ThingTemplate *structTmpl = s_scenario.capBuildStructure.isEmpty() ? nullptr : TheThingFactory->findTemplate(s_scenario.capBuildStructure);
			const ThingTemplate *builderTmpl = s_scenario.capBuildBuilder.isEmpty() ? nullptr : TheThingFactory->findTemplate(s_scenario.capBuildBuilder);
			std::vector<Object *> builders;
			if (structTmpl && builderTmpl)
			{
				for (Int b = 0; b < s_scenario.capBuildCount; ++b)
				{
					Coord3D pos = lerp(s_start[0], s_centre, 0.10f);
					pos.x += (Real)b * 15.0f;
					Object *builder = spawnUnit(player, builderTmpl, pos);
					if (builder)
						builders.push_back(builder);
				}
				room += (Int)builders.size() * structTmpl->getLoadPoints();
			}

			Int spawned = 0;
			while (player->getLoadPoints() + unit->getLoadPoints() + room <= cap && spawned < 1000)
			{
				Coord3D pos;
				pos.x = anchor.x + (Real)(spawned % 20) * 12.0f;
				pos.y = anchor.y + (Real)(spawned / 20) * 12.0f;
				pos.z = 0.0f;
				if (!spawnUnit(player, unit, pos))
					break;
				++spawned;
			}
			Int started = 0;
			for (size_t b = 0; b < builders.size(); ++b)
			{
				// spots around the base, away from the spawned units; the first legal one per builder is used
				for (Int attempt = 0; attempt < 48; ++attempt)
				{
					const Real angle = (Real)(b * 48 + attempt) * 0.61f;
					const Real dist = 120.0f + (Real)(attempt % 8) * 40.0f;
					Coord3D pos = s_start[0];
					pos.x += dist * (Real)cos(angle);
					pos.y += dist * (Real)sin(angle);
					pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
					const LegalBuildCode lbc = TheBuildAssistant->isLocationLegalToBuild(&pos, structTmpl, 0.0f,
						BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::CLEAR_PATH | BuildAssistant::NO_OBJECT_OVERLAP |
						BuildAssistant::SHROUD_REVEALED, builders[b], nullptr);
					if (b == 0 && attempt == 0)
						Bench::setInfoInt("cap_build_first_lbc", (Int)lbc);
					if (lbc != LBC_OK)
						continue;
					if (TheBuildAssistant->buildObjectNow(builders[b], structTmpl, &pos, 0.0f, player))
						++started;
					break;
				}
			}
			const Int loadBeforeQueue = player->getLoadPoints();
			Int queued = 0;
			ProductionUpdateInterface *pui = factory->getProductionUpdateInterface();
			for (Int q = 0; pui && q < s_scenario.capQueue + 2; ++q)
			{
				if (pui->queueCreateUnit(unit, pui->requestUniqueUnitID()))
					++queued;
			}
			Bench::setInfoInt("cap_build_builders", (Int)builders.size());
			Bench::setInfoInt("cap_build_started", started);
			Bench::setInfoInt("cap_queue_can_make", (Int)TheBuildAssistant->canMakeUnit(factory, unit));
			Bench::setInfoInt("cap_queue_money", (Int)player->getMoney()->countMoney());
			Bench::setInfoInt("cap_queue_cap", cap);
			Bench::setInfoInt("cap_queue_spawned", spawned);
			Bench::setInfoInt("cap_queue_load_before_queue", loadBeforeQueue);
			Bench::setInfoInt("cap_queue_queued", queued);
			Bench::setInfoInt("cap_queue_load_after_queue", player->getLoadPoints());
			s_capTraceActive = TRUE;
			s_capTraceFactory = factory->getID();
			return;
		}

		const Int loadBefore = player->getLoadPoints();
		const Bool canBefore = player->canBuildMoreOfType(unit);
		const CanMakeType makeBeforeType = TheBuildAssistant->canMakeUnit(factory, unit);
		const Bool makeBefore = makeBeforeType == CANMAKE_OK;

		// Fill the player up to the cap with directly spawned units (the driver bypasses the cap on purpose).
		const Coord3D anchor = lerp(s_start[0], s_centre, 0.18f);
		Int spawned = 0;
		while (player->getLoadPoints() + unit->getLoadPoints() <= cap && spawned < 1000)
		{
			Coord3D pos;
			pos.x = anchor.x + (Real)(spawned % 20) * 12.0f;
			pos.y = anchor.y + (Real)(spawned / 20) * 12.0f;
			pos.z = 0.0f;
			if (!spawnUnit(player, unit, pos))
				break;
			++spawned;
		}
		const Int loadAtCap = player->getLoadPoints();
		const Bool canAtCap = player->canBuildMoreOfType(unit);
		const CanMakeType makeAtCap = TheBuildAssistant->canMakeUnit(factory, unit);

		Bench::setInfoInt("cap_test_cap", cap);
		Bench::setInfoInt("cap_test_unit_points", unit->getLoadPoints());
		Bench::setInfoInt("cap_test_load_before", loadBefore);
		Bench::setInfoInt("cap_test_can_build_before", canBefore ? 1 : 0);
		Bench::setInfoInt("cap_test_can_make_before", (Int)makeBeforeType);
		Bench::setInfoInt("cap_test_spawned", spawned);
		Bench::setInfoInt("cap_test_load_at_cap", loadAtCap);
		Bench::setInfoInt("cap_test_can_build_at_cap", canAtCap ? 1 : 0);
		Bench::setInfoInt("cap_test_can_make_at_cap", (Int)makeAtCap);
		// FORK @feature 09/10/2026 Units that appear without production (special powers, scripts) may push the load over
		// the cap; production must then stay refused. Spawn 4 more past the cap, the way a paradrop would.
		Int spawnedOver = 0;
		for (Int extra = 0; extra < 4; ++extra)
		{
			Coord3D pos;
			pos.x = anchor.x + (Real)((spawned + extra) % 20) * 12.0f;
			pos.y = anchor.y + (Real)((spawned + extra) / 20) * 12.0f;
			pos.z = 0.0f;
			if (spawnUnit(player, unit, pos))
				++spawnedOver;
		}
		const Int loadOver = player->getLoadPoints();
		const Bool canOver = player->canBuildMoreOfType(unit);
		const CanMakeType makeOver = TheBuildAssistant->canMakeUnit(factory, unit);
		Bench::setInfoInt("cap_test_spawned_over", spawnedOver);
		Bench::setInfoInt("cap_test_load_over", loadOver);
		Bench::setInfoInt("cap_test_can_build_over", canOver ? 1 : 0);
		Bench::setInfoInt("cap_test_can_make_over", (Int)makeOver);

		const Bool ok = cap > 0 && canBefore && makeBefore && !canAtCap && makeAtCap == CANMAKE_MAXED_OUT_FOR_PLAYER
			&& loadAtCap <= cap && loadAtCap + unit->getLoadPoints() > cap
			&& spawnedOver == 4 && loadOver > cap && !canOver && makeOver == CANMAKE_MAXED_OUT_FOR_PLAYER;
		Bench::setInfoInt("cap_test_ok", ok ? 1 : 0);
	}

	void setup()
	{
		s_setupFrame = TheGameLogic->getFrame();

		TheVictoryConditions->setVictoryConditions(0);
		deactivateAllScripts();

		Real cx = 0.0f;
		Real cy = 0.0f;
		for (Int i = 0; i < s_scenario.players; ++i)
		{
			s_players[i] = ThePlayerList->getPlayerFromSlotIndex(i);
			s_laneStructures[i] = 0;
			if (!s_players[i])
			{
				AsciiString err;
				err.format("no player for slot %d", i);
				fail(err.str());
				return;
			}
			s_players[i]->deletePlayerAI();

			AsciiString wpName;
			wpName.format("Player_%d_Start", i + 1);
			Waypoint *wp = TheTerrainLogic->getWaypointByName(wpName);
			if (!wp)
			{
				AsciiString err;
				err.format("map has no waypoint %s", wpName.str());
				fail(err.str());
				return;
			}
			s_start[i] = *wp->getLocation();
			cx += s_start[i].x;
			cy += s_start[i].y;
		}
		s_centre.x = cx / (Real)s_scenario.players;
		s_centre.y = cy / (Real)s_scenario.players;
		s_centre.z = TheTerrainLogic->getGroundHeight(s_centre.x, s_centre.y);

		s_chokes.clear();
		for (size_t c = 0; c < s_scenario.chokeWaypoints.size(); ++c)
		{
			Waypoint *wp = TheTerrainLogic->getWaypointByName(s_scenario.chokeWaypoints[c]);
			if (wp)
				s_chokes.push_back(*wp->getLocation());
		}
		Bench::setInfoInt("choke_waypoints_found", (Int)s_chokes.size());

		// Extra base structures in a ring behind the command center.
		for (Int i = 0; i < s_scenario.players; ++i)
		{
			const BenchFactionSetup *fs = s_scenario.findFaction(s_scenario.slotFactions[i % s_scenario.slotFactions.size()]);
			const Real awayX = s_start[i].x - s_centre.x;
			const Real awayY = s_start[i].y - s_centre.y;
			const Real len = (Real)sqrt(awayX * awayX + awayY * awayY);
			const Real baseAngle = len > 1.0f ? (Real)atan2(awayY, awayX) : 0.0f;
			for (size_t k = 0; k < fs->structures.size(); ++k)
			{
				const Real a = baseAngle + ((Real)k - (Real)(fs->structures.size() - 1) * 0.5f) * 0.6f;
				Coord3D pos;
				pos.x = s_start[i].x + (Real)cos(a) * 160.0f;
				pos.y = s_start[i].y + (Real)sin(a) * 160.0f;
				pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
				placeStructure(s_players[i], fs->structures[k], pos);
			}
		}

		// Loading and setup work is not part of the measured frames.
		Bench::resetAccumulators();
		s_setupDone = TRUE;
	}

	void countUnits(Int *liveUnits, Int &totalObjects, Int &totalUnits, Int &waiting, Bool sampleWaiting)
	{
		for (Int i = 0; i < MAX_SLOTS; ++i)
			liveUnits[i] = 0;
		totalObjects = 0;
		totalUnits = 0;
		waiting = 0;
		const UnsignedInt frame = TheGameLogic->getFrame();
		std::map<ObjectID, UnsignedInt> stillWaiting;
		for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		{
			++totalObjects;
			if (!isArmyUnit(obj))
				continue;
			++totalUnits;
			const Int slot = slotOfPlayer(obj->getControllingPlayer());
			if (slot >= 0)
				++liveUnits[slot];

			if (sampleWaiting)
			{
				const AIUpdateInterface *ai = obj->getAIUpdateInterface();
				if (ai && ai->isWaitingForPath())
				{
					std::map<ObjectID, UnsignedInt>::const_iterator it = s_waitingSince.find(obj->getID());
					const UnsignedInt since = (it != s_waitingSince.end()) ? it->second : frame;
					stillWaiting[obj->getID()] = since;
					if (frame - since >= FREEZE_FRAMES)
						++waiting;
				}
			}
		}
		if (sampleWaiting)
			s_waitingSince.swap(stillWaiting);
	}

	void spawnWaves(const Int *liveUnits)
	{
		for (Int i = 0; i < s_scenario.players; ++i)
		{
			const BenchFactionSetup *fs = s_scenario.findFaction(s_scenario.slotFactions[i % s_scenario.slotFactions.size()]);
			Int live = liveUnits[i];
			if (live >= s_scenario.armyCap)
				continue;

			// Spawn point: between the base and the map centre, so units leave the base area.
			const Coord3D anchor = lerp(s_start[i], s_centre, 0.18f);
			Int k = 0;
			for (size_t w = 0; w < fs->wave.size() && live < s_scenario.armyCap; ++w)
			{
				const ThingTemplate *tmpl = TheThingFactory->findTemplate(fs->wave[w].templateName);
				if (!tmpl)
					continue;
				for (Int c = 0; c < fs->wave[w].count && live < s_scenario.armyCap; ++c, ++k)
				{
					// Sunflower spiral around the anchor keeps units apart without randomness.
					const Real a = (Real)k * 2.39996f;
					const Real r = 15.0f + 12.0f * (Real)sqrt((Real)k);
					Coord3D pos;
					pos.x = anchor.x + (Real)cos(a) * r;
					pos.y = anchor.y + (Real)sin(a) * r;
					pos.z = 0.0f;
					if (spawnUnit(s_players[i], tmpl, pos))
						++live;
				}
			}
		}
	}

	Coord3D pickTarget(Int slot, Int chunk)
	{
		if (!s_chokes.empty() && (chunk % 5) == 4)
			return s_chokes[(chunk / 5) % s_chokes.size()];
		if ((chunk % 3) == 2)
			return s_centre;
		Int enemy = (slot + 1 + chunk) % s_scenario.players;
		if (enemy == slot)
			enemy = (enemy + 1) % s_scenario.players;
		return s_start[enemy];
	}

	void issueOrders(Int slot)
	{
		Player *player = s_players[slot];
		std::vector<ObjectID> idle;
		for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
		{
			if (obj->getControllingPlayer() != player || !isArmyUnit(obj))
				continue;
			const AIUpdateInterface *ai = obj->getAIUpdateInterface();
			if (ai && ai->isIdle())
				idle.push_back(obj->getID());
		}

		const Int playerIndex = player->getPlayerIndex();
		Int chunk = 0;
		for (size_t first = 0; first < idle.size(); first += (size_t)s_scenario.groupSize, ++chunk)
		{
			GameMessage *select = newInstance(GameMessage)(GameMessage::MSG_CREATE_SELECTED_GROUP);
			select->friend_setPlayerIndex(playerIndex);
			select->appendBooleanArgument(TRUE);
			for (size_t j = first; j < idle.size() && j < first + (size_t)s_scenario.groupSize; ++j)
				select->appendObjectIDArgument(idle[j]);
			TheCommandList->appendMessage(select);

			const Coord3D target = pickTarget(slot, chunk + (Int)(TheGameLogic->getFrame() / s_scenario.orderEvery));
			GameMessage *move = newInstance(GameMessage)(GameMessage::MSG_DO_ATTACKMOVETO);
			move->friend_setPlayerIndex(playerIndex);
			move->appendLocationArgument(target);
			TheCommandList->appendMessage(move);
		}
	}

	void placeLaneStructures()
	{
		for (Int i = 0; i < s_scenario.players; ++i)
		{
			if (s_laneStructures[i] >= s_scenario.maxLaneStructures)
				continue;
			const BenchFactionSetup *fs = s_scenario.findFaction(s_scenario.slotFactions[i % s_scenario.slotFactions.size()]);
			if (fs->structures.empty())
				continue;
			const Int n = s_laneStructures[i];
			const Coord3D onLane = lerp(s_start[i], s_centre, 0.30f + 0.08f * (Real)(n % 4));
			const Real dx = s_centre.x - s_start[i].x;
			const Real dy = s_centre.y - s_start[i].y;
			const Real len = (Real)sqrt(dx * dx + dy * dy);
			Coord3D pos = onLane;
			if (len > 1.0f)
			{
				const Real side = (n % 2) ? 70.0f : -70.0f;
				pos.x += -dy / len * side;
				pos.y += dx / len * side;
				pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
			}
			placeStructure(s_players[i], fs->structures[n % fs->structures.size()], pos);
			++s_laneStructures[i];
		}
	}
}

void BenchDriver::onEngineInit()
{
	if (!Bench::s_active)
		return;

	if (Bench::getOutDir()[0] == '\0')
	{
		AsciiString dir = TheGlobalData->getPath_UserData();
		dir.concat("bench_out");
		Bench::setOutDir(dir.str());
	}

	Bench::setInfoInt("audio_on", TheGlobalData->m_audioOn ? 1 : 0);

	if (Bench::getListMapsFile()[0] != '\0')
	{
		writeMapList(Bench::getListMapsFile());
		s_finished = TRUE;
		TheGameEngine->setQuitting(TRUE);
		return;
	}

	AsciiString error;
	if (!s_scenario.parse(Bench::getScenario(), error) || !startGame(error))
	{
		fail(error.str());
		return;
	}
	// Windowed runs must really draw: a minimized window skips W3DDisplay::draw entirely.
	if (!TheGlobalData->m_headless && ApplicationHWnd)
	{
		::ShowWindow(ApplicationHWnd, SW_RESTORE);
		::SetForegroundWindow(ApplicationHWnd);
	}
	printf("BENCH START: %s on %s, %d players, %u frames\n", s_scenario.name.str(), s_scenario.map.str(), s_scenario.players, s_scenario.frames);
	fflush(stdout);
}

void BenchDriver::preLogicUpdate()
{
	// The windowed client also sends its own network commands (replay camera, CRC, retaliation option).
	// Drop them so a windowed run plays exactly the same game as a headless run; only the driver plays.
	// This runs before the driver appends its own commands for this frame.
	for (GameMessage *msg = TheCommandList->getFirstMessage(); msg; )
	{
		GameMessage *next = msg->next();
		if (msg->getType() > GameMessage::MSG_BEGIN_NETWORK_MESSAGES && msg->getType() < GameMessage::MSG_END_NETWORK_MESSAGES)
		{
			TheCommandList->removeMessage(msg);
			deleteInstance(msg);
		}
		msg = next;
	}

	if (s_finished)
		return;

	if (!s_setupDone && ++s_updatesBeforeSetup > MAX_UPDATES_BEFORE_SETUP)
	{
		fail("the bench game did not start");
		return;
	}

	if (!TheGameLogic->isInGame() || TheGameLogic->isInShellGame())
		return;

	if (!s_setupDone)
	{
		// The game mode is set one update before the map loads; wait until the slot players exist.
		if (TheGameLogic->isLoadingMap() || ThePlayerList->getPlayerFromSlotIndex(0) == nullptr)
			return;
		setup();
		if (!s_setupDone)
			return;
	}

	const UnsignedInt frame = TheGameLogic->getFrame();

	// The cap test runs once the base structures placed at setup have finished their construction status.
	if (frame == s_setupFrame + 60)
		runCapTest();

	if ((frame % s_scenario.waveEvery) == 0)
	{
		Int liveUnits[MAX_SLOTS];
		Int totalObjects, totalUnits, waiting;
		countUnits(liveUnits, totalObjects, totalUnits, waiting, FALSE);
		spawnWaves(liveUnits);
	}

	for (Int i = 0; i < s_scenario.players; ++i)
	{
		if ((frame % s_scenario.orderEvery) == ((UnsignedInt)i * 37u) % s_scenario.orderEvery)
			issueOrders(i);
	}

	if ((frame % s_scenario.structureEvery) == 0)
		placeLaneStructures();

	// Windowed runs keep the camera on the local player's spawn area (outside its shroud), where its waves gather
	// and enemy attacks arrive, so the render cost is measured on visible units. Client only: logic never reads it.
	if (!TheGlobalData->m_headless && TheTacticalView && (frame % 150) == 0)
	{
		const Coord3D watch = lerp(s_start[0], s_centre, 0.18f);
		TheTacticalView->lookAt(&watch);
	}
}

void BenchDriver::postLogicUpdate()
{
	if (s_finished || !s_setupDone)
		return;

	const UnsignedInt frame = TheGameLogic->getFrame();
	if ((frame % 30) == 0)
	{
		Int liveUnits[MAX_SLOTS];
		Int totalObjects, totalUnits, waiting;
		countUnits(liveUnits, totalObjects, totalUnits, waiting, TRUE);
		Bench::setCounter(BENCHC_LIVE_UNITS, totalUnits);
		Bench::setCounter(BENCHC_OBJECTS, totalObjects);
		Bench::setCounter(BENCHC_WAITING_UNITS, waiting);

		// FORK @feature 09/10/2026 capQueue trace (load points of slot 0 and objects / queued entries).
		if (s_capTraceActive && s_players[0])
		{
			const Int load = s_players[0]->getLoadPoints();
			Object *factory = TheGameLogic->findObjectByID(s_capTraceFactory);
			ProductionUpdateInterface *pui = factory ? factory->getProductionUpdateInterface() : nullptr;
			AsciiString sample;
			sample.format("%s%d/q%d", s_capTrace.isEmpty() ? "" : " ", load, pui ? (Int)pui->getProductionCount() : -1);
			s_capTrace.concat(sample);
			if (load < s_capTraceMin) s_capTraceMin = load;
			if (load > s_capTraceMax) s_capTraceMax = load;
			Bench::setInfo("cap_queue_trace", s_capTrace.str());
			Bench::setInfoInt("cap_queue_trace_min", s_capTraceMin);
			Bench::setInfoInt("cap_queue_trace_max", s_capTraceMax);
		}
	}

	Bench::endFrame(frame);

	if ((frame % s_scenario.crcEvery) == 0)
		Bench::recordCRC(frame, TheGameLogic->getCRC(CRC_RECALC));

	if (frame - s_setupFrame >= s_scenario.frames)
		finish();
}

Int BenchDriver::runHeadless()
{
	while (!s_finished && !TheGameEngine->getQuitting())
	{
		preLogicUpdate();
		Bench::beginSection(BENCH_FRAME_TOTAL);
		TheGameLogic->UPDATE();
		Bench::endSection(BENCH_FRAME_TOTAL);
		postLogicUpdate();
	}
	return s_exitCode;
}

Int BenchDriver::getExitCode()
{
	return s_exitCode;
}
