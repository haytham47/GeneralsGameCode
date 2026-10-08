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

void PathfindOpenList::place(Int index, const Entry& entry)
{
	m_heap[index] = entry;
	entry.cell->setHeapIndex(index);
}

void PathfindOpenList::siftUp(Int index)
{
	const Entry entry = m_heap[index];
	while (index > 0)
	{
		const Int parent = (index - 1) / 2;
		if (!isBefore(entry, m_heap[parent]))
			break;
		place(index, m_heap[parent]);
		index = parent;
	}
	place(index, entry);
}

void PathfindOpenList::siftDown(Int index)
{
	const Int count = (Int)m_heap.size();
	const Entry entry = m_heap[index];
	for (;;)
	{
		const Int left = index * 2 + 1;
		if (left >= count)
			break;
		Int child = left;
		const Int right = left + 1;
		if (right < count && isBefore(m_heap[right], m_heap[left]))
			child = right;
		if (!isBefore(m_heap[child], entry))
			break;
		place(index, m_heap[child]);
		index = child;
	}
	place(index, entry);
}

void PathfindOpenList::insert(PathfindCell* cell)
{
	// Same order as the sorted list: lower total cost first, first in first out among equal costs.
	Entry entry;
	entry.cost = cell->getTotalCost();
	entry.serial = m_serial++;
	entry.cell = cell;
	cell->setOpenSerial(entry.serial);
	m_heap.push_back(entry);
	siftUp((Int)m_heap.size() - 1);
}

void PathfindOpenList::remove(PathfindCell* cell)
{
	const Int index = cell->getHeapIndex();
	const Int last = (Int)m_heap.size() - 1;
	DEBUG_ASSERTCRASH(index >= 0 && index <= last && m_heap[index].cell == cell, ("Cell is not in the open list heap."));
	cell->setHeapIndex(-1);
	if (index == last)
	{
		m_heap.pop_back();
		return;
	}
	const Entry moved = m_heap[last];
	m_heap.pop_back();
	place(index, moved);
	if (index > 0 && isBefore(moved, m_heap[(index - 1) / 2]))
		siftUp(index);
	else
		siftDown(index);
}
#endif
