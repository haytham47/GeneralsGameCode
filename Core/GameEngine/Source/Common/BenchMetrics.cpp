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

#include "PreRTS.h"

#include "Common/BenchMetrics.h"

#include <psapi.h>
#include <algorithm>
#include <string>
#include <vector>

#pragma comment(lib, "psapi.lib")

namespace
{
	struct BenchRow
	{
		UnsignedInt frame;
		float ms[BENCH_SECTION_COUNT];
		Int counters[BENCH_COUNTER_COUNT];
	};

	struct BenchCRC
	{
		UnsignedInt frame;
		UnsignedInt crc;
	};

	struct BenchInfo
	{
		std::string key;
		std::string value;
		Bool isNumber;
	};

	std::string s_scenario;
	std::string s_outDir;
	std::string s_listMapsFile;

	LARGE_INTEGER s_freq;
	Bool s_freqInit = FALSE;
	LARGE_INTEGER s_start[BENCH_SECTION_COUNT];
	Int s_depth[BENCH_SECTION_COUNT];
	double s_accum[BENCH_SECTION_COUNT];
	Int s_counters[BENCH_COUNTER_COUNT];

	std::vector<BenchRow> s_rows;
	std::vector<BenchCRC> s_crcs;
	std::vector<BenchCRC> s_messages; // frame, (type << 8) | player
	std::vector<BenchInfo> s_info;

	UnsignedInt s_wallStartMs = 0;
	SIZE_T s_peakPrivateBytes = 0;
	unsigned long long s_peakVirtualUsed = 0;

	const char *const s_sectionNames[BENCH_SECTION_COUNT] =
	{
		"logic_total",
		"cmd_processing",
		"pathfind_queue",
		"ai_total",
		"object_updates",
		"partition",
		"scripts",
		"client_update",
		"frame_total",
	};

	const char *const s_counterNames[BENCH_COUNTER_COUNT] =
	{
		"path_queued",
		"path_dropped",
		"path_served",
		"cells",
		"pool_exhausted",
		"zone_recalc",
		"live_units",
		"objects",
		"waiting_units",
		"render_frames",
	};

	void initFreq()
	{
		if (!s_freqInit)
		{
			QueryPerformanceFrequency(&s_freq);
			s_freqInit = TRUE;
			s_wallStartMs = timeGetTime();
		}
	}

	void sampleMemory()
	{
		PROCESS_MEMORY_COUNTERS_EX pmc;
		memset(&pmc, 0, sizeof(pmc));
		pmc.cb = sizeof(pmc);
		if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof(pmc)))
		{
			if (pmc.PrivateUsage > s_peakPrivateBytes)
				s_peakPrivateBytes = pmc.PrivateUsage;
		}
		MEMORYSTATUSEX ms;
		memset(&ms, 0, sizeof(ms));
		ms.dwLength = sizeof(ms);
		if (GlobalMemoryStatusEx(&ms))
		{
			const unsigned long long used = ms.ullTotalVirtual - ms.ullAvailVirtual;
			if (used > s_peakVirtualUsed)
				s_peakVirtualUsed = used;
		}
	}

	std::string joinPath(const std::string &dir, const char *file)
	{
		std::string p = dir.empty() ? std::string(".") : dir;
		if (p[p.size() - 1] != '\\' && p[p.size() - 1] != '/')
			p += '\\';
		p += file;
		return p;
	}

	std::string jsonEscape(const std::string &s)
	{
		std::string out;
		for (size_t i = 0; i < s.size(); ++i)
		{
			const char c = s[i];
			if (c == '"' || c == '\\')
			{
				out += '\\';
				out += c;
			}
			else if ((unsigned char)c < 0x20)
			{
				out += ' ';
			}
			else
			{
				out += c;
			}
		}
		return out;
	}

	double percentile(std::vector<double> &v, double p)
	{
		if (v.empty())
			return 0.0;
		std::sort(v.begin(), v.end());
		size_t idx = (size_t)(p * (double)(v.size() - 1) + 0.5);
		if (idx >= v.size())
			idx = v.size() - 1;
		return v[idx];
	}

	void createDirs(const std::string &dir)
	{
		std::string partial;
		for (size_t i = 0; i < dir.size(); ++i)
		{
			const char c = dir[i];
			partial += c;
			if ((c == '\\' || c == '/') && partial.size() > 3)
				CreateDirectoryA(partial.c_str(), nullptr);
		}
		CreateDirectoryA(dir.c_str(), nullptr);
	}
}

namespace Bench
{
	Bool s_active = FALSE;

	void setScenario(const char *path) { s_scenario = path ? path : ""; }
	const char *getScenario() { return s_scenario.c_str(); }
	void setOutDir(const char *path) { s_outDir = path ? path : ""; }
	const char *getOutDir() { return s_outDir.c_str(); }
	void setListMapsFile(const char *path) { s_listMapsFile = path ? path : ""; }
	const char *getListMapsFile() { return s_listMapsFile.c_str(); }

	void beginSection(BenchSection s)
	{
		initFreq();
		if (s_depth[s]++ == 0)
			QueryPerformanceCounter(&s_start[s]);
	}

	void endSection(BenchSection s)
	{
		if (s_depth[s] <= 0)
			return;
		if (--s_depth[s] == 0)
		{
			LARGE_INTEGER now;
			QueryPerformanceCounter(&now);
			s_accum[s] += (double)(now.QuadPart - s_start[s].QuadPart) * 1000.0 / (double)s_freq.QuadPart;
		}
	}

	void addCounter(BenchCounter c, Int n) { s_counters[c] += n; }
	void setCounter(BenchCounter c, Int n) { s_counters[c] = n; }

	void endFrame(UnsignedInt logicFrame)
	{
		initFreq();
		BenchRow row;
		row.frame = logicFrame;
		for (Int i = 0; i < BENCH_SECTION_COUNT; ++i)
		{
			row.ms[i] = (float)s_accum[i];
			s_accum[i] = 0.0;
		}
		for (Int i = 0; i < BENCH_COUNTER_COUNT; ++i)
		{
			row.counters[i] = s_counters[i];
			// Live unit, object and waiting counts are levels, not per-frame events.
			if (i != BENCHC_LIVE_UNITS && i != BENCHC_OBJECTS && i != BENCHC_WAITING_UNITS)
				s_counters[i] = 0;
		}
		s_rows.push_back(row);

		if ((s_rows.size() % 300) == 1)
			sampleMemory();
	}

	void recordCRC(UnsignedInt logicFrame, UnsignedInt crc)
	{
		BenchCRC c;
		c.frame = logicFrame;
		c.crc = crc;
		s_crcs.push_back(c);
	}

	void logMessage(UnsignedInt logicFrame, Int type, Int playerIndex)
	{
		BenchCRC m;
		m.frame = logicFrame;
		m.crc = ((UnsignedInt)type << 8) | ((UnsignedInt)playerIndex & 0xff);
		s_messages.push_back(m);
	}

	void setInfo(const char *key, const char *value)
	{
		for (size_t i = 0; i < s_info.size(); ++i)
		{
			if (s_info[i].key == key)
			{
				s_info[i].value = value ? value : "";
				s_info[i].isNumber = FALSE;
				return;
			}
		}
		BenchInfo info;
		info.key = key;
		info.value = value ? value : "";
		info.isNumber = FALSE;
		s_info.push_back(info);
	}

	void setInfoInt(const char *key, Int value)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%d", value);
		setInfo(key, buf);
		for (size_t i = 0; i < s_info.size(); ++i)
		{
			if (s_info[i].key == key)
				s_info[i].isNumber = TRUE;
		}
	}

	Bool writeResults(Int exitCode, const char *error)
	{
		initFreq();
		sampleMemory();
		createDirs(s_outDir);

		Bool ok = TRUE;

		// frames.csv
		{
			FILE *f = fopen(joinPath(s_outDir, "frames.csv").c_str(), "w");
			if (f)
			{
				fprintf(f, "frame");
				for (Int i = 0; i < BENCH_SECTION_COUNT; ++i)
					fprintf(f, ",%s_ms", s_sectionNames[i]);
				for (Int i = 0; i < BENCH_COUNTER_COUNT; ++i)
					fprintf(f, ",%s", s_counterNames[i]);
				fprintf(f, "\n");
				for (size_t r = 0; r < s_rows.size(); ++r)
				{
					const BenchRow &row = s_rows[r];
					fprintf(f, "%u", row.frame);
					for (Int i = 0; i < BENCH_SECTION_COUNT; ++i)
						fprintf(f, ",%.4f", row.ms[i]);
					for (Int i = 0; i < BENCH_COUNTER_COUNT; ++i)
						fprintf(f, ",%d", row.counters[i]);
					fprintf(f, "\n");
				}
				fclose(f);
			}
			else
			{
				ok = FALSE;
			}
		}

		// crc.log
		{
			FILE *f = fopen(joinPath(s_outDir, "crc.log").c_str(), "w");
			if (f)
			{
				for (size_t i = 0; i < s_crcs.size(); ++i)
					fprintf(f, "%u %08X\n", s_crcs[i].frame, s_crcs[i].crc);
				fclose(f);
			}
			else
			{
				ok = FALSE;
			}
		}

		// messages.log
		{
			FILE *f = fopen(joinPath(s_outDir, "messages.log").c_str(), "w");
			if (f)
			{
				for (size_t i = 0; i < s_messages.size(); ++i)
					fprintf(f, "%u type=%u player=%u\n", s_messages[i].frame, s_messages[i].crc >> 8, s_messages[i].crc & 0xff);
				fclose(f);
			}
		}

		// summary.json: warm-up (first 10% of frames) is excluded from the statistics.
		{
			FILE *f = fopen(joinPath(s_outDir, "summary.json").c_str(), "w");
			if (!f)
				return FALSE;

			const size_t first = s_rows.size() / 10;
			const size_t n = s_rows.size() > first ? s_rows.size() - first : 0;

			fprintf(f, "{\n");
			fprintf(f, "  \"exit_code\": %d,\n", exitCode);
			fprintf(f, "  \"error\": \"%s\",\n", jsonEscape(error ? error : "").c_str());
			fprintf(f, "  \"frames_run\": %u,\n", (unsigned)s_rows.size());
			fprintf(f, "  \"frames_measured\": %u,\n", (unsigned)n);
			fprintf(f, "  \"wall_seconds\": %.3f,\n", (double)(timeGetTime() - s_wallStartMs) / 1000.0);
			fprintf(f, "  \"peak_private_mb\": %.1f,\n", (double)s_peakPrivateBytes / (1024.0 * 1024.0));
			fprintf(f, "  \"peak_virtual_used_mb\": %.1f,\n", (double)s_peakVirtualUsed / (1024.0 * 1024.0));
			for (size_t i = 0; i < s_info.size(); ++i)
			{
				if (s_info[i].isNumber)
					fprintf(f, "  \"%s\": %s,\n", jsonEscape(s_info[i].key).c_str(), s_info[i].value.c_str());
				else
					fprintf(f, "  \"%s\": \"%s\",\n", jsonEscape(s_info[i].key).c_str(), jsonEscape(s_info[i].value).c_str());
			}

			fprintf(f, "  \"sections\": {\n");
			for (Int s = 0; s < BENCH_SECTION_COUNT; ++s)
			{
				std::vector<double> v;
				v.reserve(n);
				double sum = 0.0;
				for (size_t r = first; r < s_rows.size(); ++r)
				{
					v.push_back(s_rows[r].ms[s]);
					sum += s_rows[r].ms[s];
				}
				const double mean = n ? sum / (double)n : 0.0;
				const double p50 = percentile(v, 0.50);
				const double p95 = percentile(v, 0.95);
				const double p99 = percentile(v, 0.99);
				const double mx = v.empty() ? 0.0 : v[v.size() - 1];
				fprintf(f, "    \"%s\": { \"mean\": %.4f, \"p50\": %.4f, \"p95\": %.4f, \"p99\": %.4f, \"max\": %.4f }%s\n",
					s_sectionNames[s], mean, p50, p95, p99, mx, (s + 1 < BENCH_SECTION_COUNT) ? "," : "");
			}
			fprintf(f, "  },\n");

			fprintf(f, "  \"counters\": {\n");
			double meanLiveUnits = 0.0;
			for (Int c = 0; c < BENCH_COUNTER_COUNT; ++c)
			{
				double sum = 0.0;
				Int mx = 0;
				for (size_t r = first; r < s_rows.size(); ++r)
				{
					sum += s_rows[r].counters[c];
					if (s_rows[r].counters[c] > mx)
						mx = s_rows[r].counters[c];
				}
				const double mean = n ? sum / (double)n : 0.0;
				if (c == BENCHC_LIVE_UNITS)
					meanLiveUnits = mean;
				fprintf(f, "    \"%s\": { \"mean\": %.4f, \"total\": %.0f, \"max\": %d }%s\n",
					s_counterNames[c], mean, sum, mx, (c + 1 < BENCH_COUNTER_COUNT) ? "," : "");
			}
			fprintf(f, "  },\n");

			double logicSum = 0.0;
			for (size_t r = first; r < s_rows.size(); ++r)
				logicSum += s_rows[r].ms[BENCH_LOGIC_TOTAL];
			const double logicMean = n ? logicSum / (double)n : 0.0;
			const double hundreds = meanLiveUnits / 100.0;
			fprintf(f, "  \"mean_live_units\": %.1f,\n", meanLiveUnits);
			fprintf(f, "  \"logic_ms_per_100_units\": %.4f\n", hundreds > 1.0 ? logicMean / hundreds : logicMean);
			fprintf(f, "}\n");
			fclose(f);
		}

		return ok;
	}
}
