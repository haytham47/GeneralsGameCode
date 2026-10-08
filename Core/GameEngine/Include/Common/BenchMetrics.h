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

// FORK @feature 08/10/2026 Records per-frame timings and counters for the automated -bench mode.
// Probes only read state and measure time. They must never influence game logic.

#pragma once

enum BenchSection
{
	BENCH_LOGIC_TOTAL,
	BENCH_CMD_PROCESSING,
	BENCH_PATHFIND_QUEUE,
	BENCH_AI_PLAYERS,
	BENCH_OBJECT_UPDATES,
	BENCH_PARTITION,
	BENCH_SCRIPTS,
	BENCH_CLIENT_UPDATE,
	BENCH_FRAME_TOTAL,

	BENCH_SECTION_COUNT
};

enum BenchCounter
{
	BENCHC_PATH_QUEUED,
	BENCHC_PATH_DROPPED,
	BENCHC_PATH_SERVED,
	BENCHC_CELLS,
	BENCHC_POOL_EXHAUSTED,
	BENCHC_ZONE_RECALC,
	BENCHC_LIVE_UNITS,
	BENCHC_OBJECTS,
	BENCHC_WAITING_UNITS,
	BENCHC_RENDER_FRAMES,

	BENCH_COUNTER_COUNT
};

/// Pathfinder search kinds, measured separately in -bench mode (outermost search only).
enum BenchSearchKind
{
	BENCH_SEARCH_FIND_PATH,
	BENCH_SEARCH_CLOSEST_PATH,
	BENCH_SEARCH_ATTACK_PATH,
	BENCH_SEARCH_GROUND_PATH,
	BENCH_SEARCH_MOVE_AWAY,
	BENCH_SEARCH_PATCH_PATH,
	BENCH_SEARCH_SAFE_PATH,
	BENCH_SEARCH_PATH_COST,
	BENCH_SEARCH_HIERARCHICAL,
	BENCH_SEARCH_ADJUST_DEST,
	BENCH_SEARCH_ADJUST_POSSIBLE,
	BENCH_SEARCH_QUICK_EXISTS,
	BENCH_SEARCH_SLOW_EXISTS,

	BENCH_SEARCH_COUNT
};

namespace Bench
{
	extern Bool s_active; ///< TRUE when the exe runs in -bench mode; all probes are no-ops otherwise

	void setScenario(const char *path);
	const char *getScenario();
	void setOutDir(const char *path);
	const char *getOutDir();
	void setListMapsFile(const char *path);
	const char *getListMapsFile();

	void beginSection(BenchSection s);
	void endSection(BenchSection s);
	void addCounter(BenchCounter c, Int n);
	void resetAccumulators(); ///< drop timings and counters gathered so far in the current row
	void setCounter(BenchCounter c, Int n);

	/// Close the current row. Called once per logic frame.
	void endFrame(UnsignedInt logicFrame);
	void recordCRC(UnsignedInt logicFrame, UnsignedInt crc);
	/// Logs every command the logic processes (messages.log), to compare runs and find divergence.
	void logMessage(UnsignedInt logicFrame, Int type, Int playerIndex);
	void setInfo(const char *key, const char *value);

	void beginSearch(BenchSearchKind kind);
	void endSearch(BenchSearchKind kind);
	void addSearchCells(Int cells); ///< cells released by the current outermost search
	void setInfoInt(const char *key, Int value);

	/// Write frames.csv, summary.json and crc.log into the out dir. Returns FALSE if the files cannot be written.
	Bool writeResults(Int exitCode, const char *error);
}

class BenchSearchScope
{
public:
	BenchSearchScope(BenchSearchKind k) : m_kind(k) { if (Bench::s_active) Bench::beginSearch(m_kind); }
	~BenchSearchScope() { if (Bench::s_active) Bench::endSearch(m_kind); }

private:
	BenchSearchKind m_kind;
};

class BenchScope
{
public:
	BenchScope(BenchSection s) : m_section(s) { if (Bench::s_active) Bench::beginSection(m_section); }
	~BenchScope() { if (Bench::s_active) Bench::endSection(m_section); }

private:
	BenchSection m_section;
};
