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

#pragma once

class BenchDriver
{
public:
	/// Called from GameEngine::init after the map cache is ready. Starts the scenario or writes the map list.
	static void onEngineInit();

	/// Called right before each GameLogic update while bench mode is active.
	static void preLogicUpdate();

	/// Called right after each GameLogic update while bench mode is active.
	static void postLogicUpdate();

	/// Logic-only loop used with -headless. Returns the process exit code.
	static Int runHeadless();

	static Int getExitCode();
};
