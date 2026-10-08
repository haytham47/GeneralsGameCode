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
	void setCounter(BenchCounter c, Int n);

	/// Close the current row. Called once per logic frame.
	void endFrame(UnsignedInt logicFrame);
	void recordCRC(UnsignedInt logicFrame, UnsignedInt crc);
	void setInfo(const char *key, const char *value);
	void setInfoInt(const char *key, Int value);

	/// Write frames.csv, summary.json and crc.log into the out dir. Returns FALSE if the files cannot be written.
	Bool writeResults(Int exitCode, const char *error);
}

class BenchScope
{
public:
	BenchScope(BenchSection s) : m_section(s) { if (Bench::s_active) Bench::beginSection(m_section); }
	~BenchScope() { if (Bench::s_active) Bench::endSection(m_section); }

private:
	BenchSection m_section;
};
