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

#pragma once

#include <vector>

class PathfindCell;

// TheSuperHackers @info The PathfindCellList class acts as a new management class for the pathfindcell open and closed lists
class PathfindCellList
{
	friend class PathfindCell;

public:
	PathfindCellList() : m_head(nullptr), m_tail(nullptr) {}

#if RETAIL_COMPATIBLE_PATHFINDING
	void reset(PathfindCell* newHead = nullptr) { m_head = newHead; m_tail = nullptr; }
#else
	void reset() { m_head = nullptr; m_tail = nullptr; }
#endif

	PathfindCell* getHead() const { return m_head; }

	Bool empty() const { return m_head == nullptr; }

	Bool canReverseSort(PathfindCell& currentCell) const;

private:
	PathfindCell* m_head;
	PathfindCell* m_tail;
};

#if RETAIL_COMPATIBLE_PATHFINDING
typedef PathfindCellList PathfindOpenList;
#else
// FORK @performance 08/10/2026 The A* open list is a binary heap ordered by (total cost, insertion serial).
// This pops cells in exactly the same order as the sorted linked list (ascending cost, first in first out
// among equal costs) but inserts and removes in O(log n) instead of O(n). Huge searches no longer go quadratic.
class PathfindOpenList
{
public:
	PathfindOpenList() : m_serial(0) {}

	void reset() { m_heap.clear(); m_serial = 0; }
	PathfindCell* getHead() const { return m_heap.empty() ? nullptr : m_heap[0]; }
	Bool empty() const { return m_heap.empty(); }
	Int size() const { return (Int)m_heap.size(); }
	PathfindCell* getAt(Int index) const { return m_heap[index]; }

	void insert(PathfindCell* cell);
	void remove(PathfindCell* cell);

private:
	Bool isBefore(const PathfindCell* a, const PathfindCell* b) const;
	void place(Int index, PathfindCell* cell);
	void siftUp(Int index);
	void siftDown(Int index);

	std::vector<PathfindCell*> m_heap;
	UnsignedInt m_serial;
};
#endif
