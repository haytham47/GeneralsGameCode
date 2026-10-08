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

#include "GameLogic/Pathfinder/PathfindCell.h"
#include "GameLogic/Pathfinder/PathfindCellList.h"

Bool PathfindCellList::canReverseSort(PathfindCell& currentCell) const
{
	if (m_head && m_tail)
		return m_head->getTotalCostDifference(currentCell) > m_tail->getTotalCostDifference(currentCell);

	return false;
}

#if !RETAIL_COMPATIBLE_PATHFINDING
// FORK @performance 08/10/2026 Binary heap open list, see PathfindCellList.h.

Bool PathfindOpenList::isBefore(const PathfindCell* a, const PathfindCell* b) const
{
	// Lower total cost first; among equal costs, the cell inserted first comes first (same as the sorted list).
	if (a->getTotalCost() != b->getTotalCost())
		return a->getTotalCost() < b->getTotalCost();
	return a->getOpenSerial() < b->getOpenSerial();
}

void PathfindOpenList::place(Int index, PathfindCell* cell)
{
	m_heap[index] = cell;
	cell->setHeapIndex(index);
}

void PathfindOpenList::siftUp(Int index)
{
	PathfindCell* cell = m_heap[index];
	while (index > 0)
	{
		const Int parent = (index - 1) / 2;
		if (!isBefore(cell, m_heap[parent]))
			break;
		place(index, m_heap[parent]);
		index = parent;
	}
	place(index, cell);
}

void PathfindOpenList::siftDown(Int index)
{
	const Int count = (Int)m_heap.size();
	PathfindCell* cell = m_heap[index];
	for (;;)
	{
		const Int left = index * 2 + 1;
		if (left >= count)
			break;
		Int child = left;
		const Int right = left + 1;
		if (right < count && isBefore(m_heap[right], m_heap[left]))
			child = right;
		if (!isBefore(m_heap[child], cell))
			break;
		place(index, m_heap[child]);
		index = child;
	}
	place(index, cell);
}

void PathfindOpenList::insert(PathfindCell* cell)
{
	cell->setOpenSerial(m_serial++);
	m_heap.push_back(cell);
	siftUp((Int)m_heap.size() - 1);
}

void PathfindOpenList::remove(PathfindCell* cell)
{
	const Int index = cell->getHeapIndex();
	const Int last = (Int)m_heap.size() - 1;
	DEBUG_ASSERTCRASH(index >= 0 && index <= last && m_heap[index] == cell, ("Cell is not in the open list heap."));
	cell->setHeapIndex(-1);
	if (index == last)
	{
		m_heap.pop_back();
		return;
	}
	PathfindCell* moved = m_heap[last];
	m_heap.pop_back();
	place(index, moved);
	if (index > 0 && isBefore(moved, m_heap[(index - 1) / 2]))
		siftUp(index);
	else
		siftDown(index);
}
#endif
