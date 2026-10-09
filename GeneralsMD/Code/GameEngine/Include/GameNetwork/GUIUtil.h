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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: GUIUtil.h //////////////////////////////////////////////////////
// Author: Matthew D. Campbell, Sept 2002

#pragma once

class GameWindow;
class GameInfo;

void ShowUnderlyingGUIElements( Bool show, const char *layoutFilename, const char *parentName,
															 const char **gadgetsToHide, const char **perPlayerGadgetsToHide );

void PopulateColorComboBox(Int comboBox, GameWindow *comboArray[], GameInfo *myGame, Bool isObserver = FALSE);
void PopulatePlayerTemplateComboBox(Int comboBox, GameWindow *comboArray[], GameInfo *myGame, Bool allowObservers );
void PopulateTeamComboBox(Int comboBox, GameWindow *comboArray[], GameInfo *myGame, Bool isObserver = FALSE);
void PopulateStartingCashComboBox(GameWindow *comboBox, GameInfo *myGame);

// FORK @feature 09/10/2026 Build cap selector created at runtime (the shipped .wnd files have no cap control).
GameWindow *CreateLoadCapGadgets(GameWindow *templateComboBox, const char *templateLabelText, const char *comboBoxName,
																 Int labelLeft, Int labelRight, Int comboLeft, Int comboRight, Int top, Int bottom);
void PopulateLoadCapComboBox(GameWindow *comboBox, GameInfo *myGame);
void SelectLoadCapComboBox(GameWindow *comboBox, UnsignedInt loadCap);
UnsignedInt GetLoadCapComboBoxSelection(GameWindow *comboBox);

// FORK @feature 09/10/2026 Generic runtime lobby combo (label + combo box), Superweapons and General's points selectors.
GameWindow *CreateLobbyComboGadgets(GameWindow *templateComboBox, const char *templateLabelText, const char *comboBoxName,
																		const wchar_t *labelText, const wchar_t *tooltipText,
																		Int labelLeft, Int labelRight, Int comboLeft, Int comboRight, Int top, Int bottom);
UnsignedInt GetComboBoxSelectedItemData(GameWindow *comboBox, UnsignedInt defaultData);
void ShrinkWindowTopToLayoutY(GameWindow *window, Int layoutTop); ///< moves the top edge down to y of the 800x600 layout, bottom stays
enum
{
	SUPERWEAPONS_UNLIMITED = 0,	///< superweapons as usual
	SUPERWEAPONS_LIMITED,				///< retail "Limit Superweapons": one per type
	SUPERWEAPONS_DISABLED,			///< can be built and upgraded, never fire
	SUPERWEAPONS_MODE_COUNT
};
Int GetSuperweaponsMode(const GameInfo *myGame);
void SetSuperweaponsMode(GameInfo *myGame, Int mode);
void PopulateSuperweaponsComboBox(GameWindow *comboBox, GameInfo *myGame);
void SelectSuperweaponsComboBox(GameWindow *comboBox, const GameInfo *myGame);
void PopulateGeneralPointsRateComboBox(GameWindow *comboBox, GameInfo *myGame);
void SelectGeneralPointsRateComboBox(GameWindow *comboBox, UnsignedInt rate);

void EnableSlotListUpdates( Bool val );
Bool AreSlotListUpdatesEnabled();

void UpdateSlotList( GameInfo *myGame, GameWindow *comboPlayer[],
										GameWindow *comboColor[], GameWindow *comboPlayerTemplate[],
										GameWindow *comboTeam[], GameWindow *buttonAccept[],
										GameWindow *buttonStart, GameWindow *buttonMapStartPosition[] );

void EnableAcceptControls(Bool Enabled, GameInfo *myGame, GameWindow *comboPlayer[],
										GameWindow *comboColor[], GameWindow *comboPlayerTemplate[],
										GameWindow *comboTeam[], GameWindow *buttonAccept[], GameWindow *buttonStart,
										GameWindow *buttonMapStartPosition[], Int slotNum = -1);
