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

// FILE: GUIUtil.cpp //////////////////////////////////////////////////////
// Author: Matthew D. Campbell, Sept 2002

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameNetwork/GUIUtil.h"
#include "GameNetwork/NetworkDefs.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Display.h" // FORK @feature 09/10/2026 TheDisplay for the build cap selector layout
#include "GameClient/MapUtil.h"
#include "Common/NameKeyGenerator.h"

#include "Common/MultiplayerSettings.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameText.h"
#include "GameLogic/GameLogic.h" // SUPERWEAPON_RESTRICT_COUNT
#include "GameNetwork/GameInfo.h"
#include "Common/PlayerTemplate.h"
#include "GameNetwork/LANAPICallbacks.h" // for acceptTrueColor, etc
#include "GameClient/ChallengeGenerals.h"


// -----------------------------------------------------------------------------

static Bool winInitialized = FALSE;

void EnableSlotListUpdates( Bool val )
{
	winInitialized = val;
}

Bool AreSlotListUpdatesEnabled()
{
	return winInitialized;
}

// -----------------------------------------------------------------------------

void EnableAcceptControls(Bool Enabled, GameInfo *myGame, GameWindow *comboPlayer[],
										GameWindow *comboColor[], GameWindow *comboPlayerTemplate[],
										GameWindow *comboTeam[], GameWindow *buttonAccept[], GameWindow *buttonStart,
										GameWindow *buttonMapStartPosition[], Int slotNum)
{
	if(slotNum == -1 || slotNum >= MAX_SLOTS )
		slotNum = myGame->getLocalSlotNum();

	Bool isObserver = myGame->getConstSlot(slotNum)->getPlayerTemplate() == PLAYERTEMPLATE_OBSERVER;

	if( !myGame->amIHost() && (buttonStart != nullptr) )
		buttonStart->winEnable(Enabled);
	if(comboColor[slotNum])
	{
		if (isObserver)
		{
			GadgetComboBoxHideList(comboColor[slotNum]);
		}
		comboColor[slotNum]->winEnable(Enabled && !isObserver);
	}
	if(comboPlayerTemplate[slotNum])
		comboPlayerTemplate[slotNum]->winEnable(Enabled);
	if(comboTeam[slotNum])
	{
		if (isObserver)
		{
			GadgetComboBoxHideList(comboTeam[slotNum]);
		}
		comboTeam[slotNum]->winEnable(Enabled && !isObserver);
	}

	Bool canChooseStartSpot = FALSE;
	if (!isObserver)
		canChooseStartSpot = TRUE;
	for (Int i=0; i<MAX_SLOTS && !canChooseStartSpot && myGame->amIHost(); ++i)
	{
		if (myGame->getConstSlot(i) && myGame->getConstSlot(i)->isAI())
			canChooseStartSpot = TRUE;
	}

	if (slotNum == myGame->getLocalSlotNum())
	{
		if (myGame->getConstSlot(myGame->getLocalSlotNum())->hasMap())
		{
			for (Int i=0; i<MAX_SLOTS; ++i)
			{
				if (buttonMapStartPosition[i])
				{
					buttonMapStartPosition[i]->winEnable(Enabled && canChooseStartSpot);
				}
			}
		}
		else
		{
			for (Int i=0; i<MAX_SLOTS; ++i)
			{
				if (buttonMapStartPosition[i])
					buttonMapStartPosition[i]->winEnable(FALSE);
			}
		}
	}
}

// -----------------------------------------------------------------------------

void ShowUnderlyingGUIElements( Bool show, const char *layoutFilename, const char *parentName,
															 const char **gadgetsToHide, const char **perPlayerGadgetsToHide )
{
	AsciiString parentNameStr;
	parentNameStr.format("%s:%s", layoutFilename, parentName);
	NameKeyType parentID = NAMEKEY(parentNameStr);
	GameWindow *parent = TheWindowManager->winGetWindowFromId( nullptr, parentID );
	if (!parent)
	{
		DEBUG_CRASH(("Window %s not found", parentNameStr.str()));
		return;
	}

	// hide some GUI elements of the screen underneath
	GameWindow *win;

	Int player;
	const char **text;

	text = gadgetsToHide;
	while (*text)
	{
		AsciiString gadgetName;
		gadgetName.format("%s:%s", layoutFilename, *text);
		win	= TheWindowManager->winGetWindowFromId( parent, NAMEKEY(gadgetName) );
		//DEBUG_ASSERTCRASH(win, ("Cannot find %s to show/hide it", gadgetName.str()));
		if (win)
		{
			win->winHide( !show );
		}
		++text;
	}

	text = perPlayerGadgetsToHide;
	while (*text)
	{
		for (player = 0; player < MAX_SLOTS; ++player)
		{
			AsciiString gadgetName;
			gadgetName.format("%s:%s%d", layoutFilename, *text, player);
			win	= TheWindowManager->winGetWindowFromId( parent, NAMEKEY(gadgetName) );
			//DEBUG_ASSERTCRASH(win, ("Cannot find %s to show/hide it", gadgetName.str()));
			if (win)
			{
				win->winHide( !show );
			}
		}
		++text;
	}
}

// -----------------------------------------------------------------------------

void PopulateColorComboBox(Int comboBox, GameWindow *comboArray[], GameInfo *myGame, Bool isObserver)
{
	Int numColors = TheMultiplayerSettings->getNumColors();
	UnicodeString colorName;
	std::vector<bool> availableColors;

	Int i = 0;
	for (; i < numColors; i++)
		availableColors.push_back(true);

	for (i = 0; i < MAX_SLOTS; i++)
	{
		GameSlot *slot = myGame->getSlot(i);
		if( slot && (i != comboBox) && (slot->getColor() >= 0 )&& (slot->getColor() < numColors))
		{
			DEBUG_ASSERTCRASH(slot->getColor() >= 0,("We've tried to access array %d and that ain't good",slot->getColor()));
			availableColors[slot->getColor()] = false;
		}
	}

	Bool wasObserver = (GadgetComboBoxGetLength(comboArray[comboBox]) == 1);
	GadgetComboBoxReset(comboArray[comboBox]);

	MultiplayerColorDefinition *def = TheMultiplayerSettings->getColor(PLAYERTEMPLATE_RANDOM);
	Int newIndex = GadgetComboBoxAddEntry(comboArray[comboBox],
		(isObserver)?TheGameText->fetch("GUI:None"):TheGameText->fetch("GUI:???"), def->getColor());
	GadgetComboBoxSetItemData(comboArray[comboBox], newIndex, (void *)-1);

	if (isObserver)
	{
		GadgetComboBoxSetSelectedPos(comboArray[comboBox], 0);
		return;
	}

	for (Int c=0; c<numColors; ++c)
	{
		def = TheMultiplayerSettings->getColor(c);
		if (!def || availableColors[c] == false)
			continue;

		colorName = TheGameText->fetch(def->getTooltipName().str());
		newIndex = GadgetComboBoxAddEntry(comboArray[comboBox], colorName, def->getColor());
		GadgetComboBoxSetItemData(comboArray[comboBox], newIndex, (void *)c);
	}
	if (wasObserver)
		GadgetComboBoxSetSelectedPos(comboArray[comboBox], 0);
}

// -----------------------------------------------------------------------------

void PopulatePlayerTemplateComboBox(Int comboBox, GameWindow *comboArray[], GameInfo *myGame, Bool allowObservers)
{
	Int numPlayerTemplates = ThePlayerTemplateStore->getPlayerTemplateCount();
	UnicodeString playerTemplateName;

	GadgetComboBoxReset(comboArray[comboBox]);

	MultiplayerColorDefinition *def = TheMultiplayerSettings->getColor(PLAYERTEMPLATE_RANDOM);
	Int newIndex = GadgetComboBoxAddEntry(comboArray[comboBox], TheGameText->fetch("GUI:Random"), def->getColor());
	GadgetComboBoxSetItemData(comboArray[comboBox], newIndex, (void *)PLAYERTEMPLATE_RANDOM);

	std::set<AsciiString> seenSides;

	for (Int c=0; c<numPlayerTemplates; ++c)
	{
		const PlayerTemplate *fac = ThePlayerTemplateStore->getNthPlayerTemplate(c);
		if (!fac)
			continue;

		if (fac->getStartingBuilding().isEmpty())
			continue;

		if ( myGame->oldFactionsOnly() && !fac->isOldFaction() )
		  continue;

		// Prevent players from selecting the disabled Generals for use.
		// This is also enforced at game loading (GameLogic.cpp and UserPreferences.cpp).
		// @todo: unlock these when something rad happens
		Bool disallowLockedGenerals = TRUE;
		const GeneralPersona *general = TheChallengeGenerals->getGeneralByTemplateName(fac->getName());
		Bool startsLocked = general ? !general->isStartingEnabled() : FALSE;
		if (disallowLockedGenerals && startsLocked)
			continue;


		AsciiString side;
		side.format("SIDE:%s", fac->getSide().str());
		if (seenSides.find(side) != seenSides.end())
			continue;

		seenSides.insert(side);

		newIndex = GadgetComboBoxAddEntry(comboArray[comboBox], TheGameText->fetch(side), def->getColor());
		GadgetComboBoxSetItemData(comboArray[comboBox], newIndex, (void *)c);
	}
	seenSides.clear();

	// disabling observers for Multiplayer test
	if (allowObservers)
	{
		def = TheMultiplayerSettings->getColor(PLAYERTEMPLATE_OBSERVER);
		newIndex = GadgetComboBoxAddEntry(comboArray[comboBox], TheGameText->fetch("GUI:Observer"), def->getColor());
		GadgetComboBoxSetItemData(comboArray[comboBox], newIndex, (void *)PLAYERTEMPLATE_OBSERVER);
	}
	GadgetComboBoxSetSelectedPos(comboArray[comboBox], 0);

}

// -----------------------------------------------------------------------------

void PopulateTeamComboBox(Int comboBox, GameWindow *comboArray[], GameInfo *myGame, Bool isObserver)
{
	Int numTeams = MAX_SLOTS/2;
	UnicodeString teamName;

	GadgetComboBoxReset(comboArray[comboBox]);

	MultiplayerColorDefinition *def = TheMultiplayerSettings->getColor(PLAYERTEMPLATE_RANDOM);
	Int newIndex = GadgetComboBoxAddEntry(comboArray[comboBox], TheGameText->fetch("Team:0"), def->getColor());
	GadgetComboBoxSetItemData(comboArray[comboBox], newIndex, (void *)-1);

	if (isObserver)
	{
		GadgetComboBoxSetSelectedPos(comboArray[comboBox], 0);
		return;
	}

	for (Int c=0; c<numTeams; ++c)
	{
		AsciiString teamStr;
		teamStr.format("Team:%d", c + 1);
		teamName = TheGameText->fetch(teamStr.str());
		newIndex = GadgetComboBoxAddEntry(comboArray[comboBox], teamName, def->getColor());
		GadgetComboBoxSetItemData(comboArray[comboBox], newIndex, (void *)c);
	}
	GadgetComboBoxSetSelectedPos(comboArray[comboBox], 0);
}

// -----------------------------------------------------------------------------
static UnicodeString formatMoneyForStartingCashComboBox( const Money & moneyAmount )
{
  UnicodeString rtn;
  rtn.format( TheGameText->fetch( "GUI:StartingMoneyFormat" ), moneyAmount.countMoney() );
  return rtn;
}

void PopulateStartingCashComboBox(GameWindow *comboBox, GameInfo *myGame)
{
  GadgetComboBoxReset(comboBox);

  const MultiplayerStartingMoneyList & startingCashMap = TheMultiplayerSettings->getStartingMoneyList();
  Int currentSelectionIndex = -1;

  for (MultiplayerStartingMoneyList::const_iterator it = startingCashMap.begin(); it != startingCashMap.end(); it++ )
  {
    Int newIndex = GadgetComboBoxAddEntry(comboBox, formatMoneyForStartingCashComboBox( *it ),
                                          comboBox->winGetEnabled() ? comboBox->winGetEnabledTextColor() : comboBox->winGetDisabledTextColor());
    GadgetComboBoxSetItemData(comboBox, newIndex, (void *)it->countMoney());

    if ( myGame->getStartingCash().amountEqual( *it ) )
    {
      currentSelectionIndex = newIndex;
    }
  }

  if ( currentSelectionIndex == -1 )
  {
    DEBUG_CRASH( ("Current selection for starting cash not found in list") );
    currentSelectionIndex = GadgetComboBoxAddEntry(comboBox, formatMoneyForStartingCashComboBox( myGame->getStartingCash() ),
                                          comboBox->winGetEnabled() ? comboBox->winGetEnabledTextColor() : comboBox->winGetDisabledTextColor());
    GadgetComboBoxSetItemData(comboBox, currentSelectionIndex, (void *)myGame->getStartingCash().countMoney() );
  }

  GadgetComboBoxSetSelectedPos(comboBox, currentSelectionIndex);
}

// -----------------------------------------------------------------------------
// FORK @feature 09/10/2026 Build cap selector. The lobby layouts come from WindowZH.big and have no cap control,
// so the combo box and its label are created here as copies of existing gadgets (same images, colors and font).
// That keeps the feature in the exe: no .wnd override file to hand out to LAN players.
// -----------------------------------------------------------------------------
static const UnsignedInt s_loadCapChoices[] = { 0, 350, 450, 650, 900, 1200 }; // 0 = no limit

static void copyWindowDrawData( WinInstanceData *dst, WinInstanceData *src )
{
	for( Int i = 0; i < MAX_DRAW_DATA; i++ )
	{
		dst->m_enabledDrawData[ i ] = src->m_enabledDrawData[ i ];
		dst->m_disabledDrawData[ i ] = src->m_disabledDrawData[ i ];
		dst->m_hiliteDrawData[ i ] = src->m_hiliteDrawData[ i ];
	}
}

static void copyWindowDrawData( GameWindow *dst, GameWindow *src )
{
	if( dst && src )
		copyWindowDrawData( dst->winGetInstanceData(), src->winGetInstanceData() );
}

static void copyGadgetLook( WinInstanceData *dst, WinInstanceData *src )
{
	copyWindowDrawData( dst, src );
	dst->m_enabledText = src->m_enabledText;
	dst->m_disabledText = src->m_disabledText;
	dst->m_hiliteText = src->m_hiliteText;
	dst->m_imeCompositeText = src->m_imeCompositeText;
	dst->m_imageOffset = src->m_imageOffset;
	dst->m_font = src->m_font;
	dst->m_headerTemplateName = src->m_headerTemplateName;
	dst->m_tooltipDelay = src->m_tooltipDelay;
}

// Converts a rectangle of the 800x600 layout to a position relative to the parent window (as the .wnd loader does).
static void layoutRectToParent( GameWindow *parent, Int left, Int top, Int right, Int bottom,
																Int *x, Int *y, Int *width, Int *height )
{
	const Real xScale = (Real)TheDisplay->getWidth() / 800.0f;
	const Real yScale = (Real)TheDisplay->getHeight() / 600.0f;
	const Int screenLeft = (Int)((Real)left * xScale);
	const Int screenTop = (Int)((Real)top * yScale);
	const Int screenRight = (Int)((Real)right * xScale);
	const Int screenBottom = (Int)((Real)bottom * yScale);

	Int parentX = 0;
	Int parentY = 0;
	if( parent )
		parent->winGetScreenPosition( &parentX, &parentY );

	*x = screenLeft - parentX;
	*y = screenTop - parentY;
	*width = screenRight - screenLeft;
	*height = screenBottom - screenTop;
}

static GameWindow *findStaticTextByLabel( GameWindow *parent, const char *label )
{
	if( !parent || !label )
		return nullptr;

	for( GameWindow *child = parent->winGetChild(); child; child = child->winGetNext() )
	{
		if( BitIsSet( child->winGetStyle(), GWS_STATIC_TEXT ) &&
				child->winGetInstanceData()->m_textLabelString.compareNoCase( label ) == 0 )
			return child;
	}
	return nullptr;
}

GameWindow *CreateLoadCapGadgets(GameWindow *templateComboBox, const char *templateLabelText, const char *comboBoxName,
																 Int labelLeft, Int labelRight, Int comboLeft, Int comboRight, Int top, Int bottom)
{
	if( !templateComboBox || !comboBoxName )
		return nullptr;

	GameWindow *parent = templateComboBox->winGetParent();
	const NameKeyType comboBoxID = NAMEKEY( comboBoxName );

	// the layout may be shown again without being reloaded: reuse what we made last time
	GameWindow *existing = TheWindowManager->winGetWindowFromId( parent, comboBoxID );
	if( existing )
		return existing;

	const UnicodeString tooltip = L"Build cap per player in load points (infantry 1, structure 2, vehicle 3, aircraft 4). "
		L"The total of all players is split by faction: USA 75%, China 70%, GLA 100%.";
	Int x, y, width, height;

	// label, copied from the label next to the template combo box
	GameWindow *templateLabel = findStaticTextByLabel( parent, templateLabelText );
	TextData *templateTextData = templateLabel ? (TextData *)templateLabel->winGetUserData() : nullptr;
	if( templateLabel && templateTextData )
	{
		WinInstanceData labelInst;
		labelInst.init();
		copyGadgetLook( &labelInst, templateLabel->winGetInstanceData() );
		labelInst.m_style = templateLabel->winGetStyle();
		labelInst.m_status = templateLabel->winGetInstanceData()->m_status;
		labelInst.m_owner = templateLabel->winGetOwner();
		labelInst.setTooltipText( tooltip );

		TextData textData = *templateTextData;
		textData.text = nullptr;

		layoutRectToParent( parent, labelLeft, top, labelRight, bottom, &x, &y, &width, &height );
		GameWindow *label = TheWindowManager->gogoGadgetStaticText( parent,
			templateLabel->winGetStatus() & ~WIN_STATUS_HIDDEN, x, y, width, height,
			&labelInst, &textData, labelInst.m_font, FALSE );
		if( label )
		{
			label->winSetOwner( templateLabel->winGetOwner() );
			GadgetStaticTextSetText( label, UnicodeString( L"Build Cap:" ) );
		}
	}
	else
	{
		DEBUG_LOG(( "CreateLoadCapGadgets: no '%s' label next to the template combo box, the cap has no label", templateLabelText ));
	}

	// combo box, copied from the template combo box (same steps as the .wnd loader)
	WinInstanceData comboInst;
	comboInst.init();
	copyGadgetLook( &comboInst, templateComboBox->winGetInstanceData() );
	comboInst.m_style = templateComboBox->winGetStyle();
	comboInst.m_owner = templateComboBox->winGetOwner();
	comboInst.m_decoratedNameString = comboBoxName;
	comboInst.m_id = (Int)comboBoxID;
	comboInst.setTooltipText( tooltip );

	ComboBoxData *templateData = (ComboBoxData *)templateComboBox->winGetUserData();
	ComboBoxData comboData;
	memset( &comboData, 0, sizeof( comboData ) );
	if( templateData )
	{
		comboData.isEditable = templateData->isEditable;
		comboData.maxDisplay = templateData->maxDisplay;
		comboData.maxChars = templateData->maxChars;
		comboData.asciiOnly = templateData->asciiOnly;
		comboData.lettersAndNumbersOnly = templateData->lettersAndNumbersOnly;
	}
	else
	{
		comboData.maxDisplay = 6;
		comboData.maxChars = 16;
	}
	// gogoGadgetComboBox copies these two and deletes them
	comboData.entryData = NEW EntryData;
	memset( comboData.entryData, 0, sizeof( EntryData ) );
	comboData.entryData->aSCIIOnly = comboData.asciiOnly;
	comboData.entryData->alphaNumericalOnly = comboData.lettersAndNumbersOnly;
	comboData.entryData->maxTextLen = comboData.maxChars;
	comboData.listboxData = NEW ListboxData;
	memset( comboData.listboxData, 0, sizeof( ListboxData ) );
	comboData.listboxData->listLength = 10;
	comboData.listboxData->scrollBar = 1;
	comboData.listboxData->forceSelect = 1;
	comboData.listboxData->columns = 1;

	layoutRectToParent( parent, comboLeft, top, comboRight, bottom, &x, &y, &width, &height );
	GameWindow *comboBox = TheWindowManager->gogoGadgetComboBox( parent,
		templateComboBox->winGetStatus() & ~WIN_STATUS_HIDDEN, x, y, width, height,
		&comboInst, &comboData, comboInst.m_font, FALSE );
	if( !comboBox )
		return nullptr;

	comboBox->winSetWindowId( comboBoxID );
	comboBox->winSetOwner( templateComboBox->winGetOwner() );

	// the sub windows get the template's images too
	copyWindowDrawData( GadgetComboBoxGetDropDownButton( comboBox ), GadgetComboBoxGetDropDownButton( templateComboBox ) );
	copyWindowDrawData( GadgetComboBoxGetEditBox( comboBox ), GadgetComboBoxGetEditBox( templateComboBox ) );
	GameWindow *listBox = GadgetComboBoxGetListBox( comboBox );
	GameWindow *templateListBox = GadgetComboBoxGetListBox( templateComboBox );
	if( listBox && templateListBox )
	{
		copyWindowDrawData( listBox, templateListBox );
		copyWindowDrawData( GadgetListBoxGetUpButton( listBox ), GadgetListBoxGetUpButton( templateListBox ) );
		copyWindowDrawData( GadgetListBoxGetDownButton( listBox ), GadgetListBoxGetDownButton( templateListBox ) );
		GameWindow *slider = GadgetListBoxGetSlider( listBox );
		GameWindow *templateSlider = GadgetListBoxGetSlider( templateListBox );
		copyWindowDrawData( slider, templateSlider );
		if( slider && templateSlider )
			copyWindowDrawData( slider->winGetChild(), templateSlider->winGetChild() );
	}

	return comboBox;
}

static UnicodeString formatLoadCapForComboBox( UnsignedInt loadCap )
{
	UnicodeString rtn;
	if( loadCap == 0 )
		rtn = L"No limit";
	else
		rtn.format( L"%u", loadCap );
	return rtn;
}

void PopulateLoadCapComboBox(GameWindow *comboBox, GameInfo *myGame)
{
	if( !comboBox )
		return;

	GadgetComboBoxReset( comboBox );
	const Color color = comboBox->winGetEnabled() ? comboBox->winGetEnabledTextColor() : comboBox->winGetDisabledTextColor();
	const UnsignedInt current = myGame ? myGame->getLoadCap() : 0;
	Bool sawCurrent = FALSE;

	for( Int i = 0; i < ARRAY_SIZE( s_loadCapChoices ); ++i )
	{
		Int newIndex = GadgetComboBoxAddEntry( comboBox, formatLoadCapForComboBox( s_loadCapChoices[i] ), color );
		GadgetComboBoxSetItemData( comboBox, newIndex, (void *)s_loadCapChoices[i] );
		if( s_loadCapChoices[i] == current )
			sawCurrent = TRUE;
	}

	// a custom value from Options.ini LoadCap stays selectable
	if( !sawCurrent )
	{
		Int newIndex = GadgetComboBoxAddEntry( comboBox, formatLoadCapForComboBox( current ), color );
		GadgetComboBoxSetItemData( comboBox, newIndex, (void *)current );
	}

	SelectLoadCapComboBox( comboBox, current );
}

void SelectLoadCapComboBox(GameWindow *comboBox, UnsignedInt loadCap)
{
	if( !comboBox )
		return;

	const Int itemCount = GadgetComboBoxGetLength( comboBox );
	for( Int index = 0; index < itemCount; ++index )
	{
		if( (UnsignedInt)GadgetComboBoxGetItemData( comboBox, index ) == loadCap )
		{
			GadgetComboBoxSetSelectedPos( comboBox, index, TRUE );
			return;
		}
	}

	// the host picked a value this list does not have (custom Options.ini LoadCap): add it
	const Color color = comboBox->winGetEnabled() ? comboBox->winGetEnabledTextColor() : comboBox->winGetDisabledTextColor();
	Int newIndex = GadgetComboBoxAddEntry( comboBox, formatLoadCapForComboBox( loadCap ), color );
	GadgetComboBoxSetItemData( comboBox, newIndex, (void *)loadCap );
	GadgetComboBoxSetSelectedPos( comboBox, newIndex, TRUE );
}

UnsignedInt GetLoadCapComboBoxSelection(GameWindow *comboBox)
{
	Int selIndex = -1;
	if( comboBox )
		GadgetComboBoxGetSelectedPos( comboBox, &selIndex );
	if( selIndex < 0 )
		return 0;
	return (UnsignedInt)GadgetComboBoxGetItemData( comboBox, selIndex );
}

// -----------------------------------------------------------------------------

//  -----------------------------------------------------------------------------------------
// The slot list displaying function
//-------------------------------------------------------------------------------------------------
void UpdateSlotList( GameInfo *myGame, GameWindow *comboPlayer[],
										GameWindow *comboColor[], GameWindow *comboPlayerTemplate[],
										GameWindow *comboTeam[], GameWindow *buttonAccept[],
										GameWindow *buttonStart, GameWindow *buttonMapStartPosition[] )
{
	if(!AreSlotListUpdatesEnabled())
		return;
	//LANGameInfo *myGame = TheLAN->GetMyGame();

	const MapMetaData *mapData = TheMapCache->findMap( myGame->getMap() );
	Bool willTransfer = TRUE;
	if (mapData)
	{
		willTransfer = !mapData->m_isOfficial;
	}
	else
	{
		willTransfer = WouldMapTransfer(myGame->getMap());
	}

	if (myGame)
	{
		for( int i =0; i < MAX_SLOTS; i++ )
		{
			GameSlot * slot = myGame->getSlot(i);

			// if i'm host, enable the controls for AI
			if(myGame->amIHost() && slot->isAI())
			{
				EnableAcceptControls(TRUE, myGame, comboPlayer, comboColor, comboPlayerTemplate,
					comboTeam, buttonAccept, buttonStart, buttonMapStartPosition, i);
			}
			else if (myGame->getLocalSlotNum() == i)
			{
				if(slot->isAccepted() && !myGame->amIHost())
				{
					EnableAcceptControls(FALSE, myGame, comboPlayer, comboColor, comboPlayerTemplate,
						comboTeam, buttonAccept, buttonStart, buttonMapStartPosition);
				}
				else
				{
					if (slot->hasMap()) {
						EnableAcceptControls(TRUE, myGame, comboPlayer, comboColor, comboPlayerTemplate,
							comboTeam, buttonAccept, buttonStart, buttonMapStartPosition);
					}
					else
					{
						EnableAcceptControls(willTransfer, myGame, comboPlayer, comboColor, comboPlayerTemplate,
							comboTeam, buttonAccept, buttonStart, buttonMapStartPosition);
					}
				}

			}
			else if(myGame->amIHost())
			{
				EnableAcceptControls(FALSE, myGame, comboPlayer, comboColor, comboPlayerTemplate,
					comboTeam, buttonAccept, buttonStart, buttonMapStartPosition, i);
			}
			if(slot->isHuman())
			{
				UnicodeString newName = slot->getName();
				UnicodeString oldName = GadgetComboBoxGetText(comboPlayer[i]);
				if (comboPlayer[i] && newName.compare(oldName))
				{
					GadgetComboBoxSetText(comboPlayer[i], newName);
				}
				if(i!= 0 && buttonAccept && buttonAccept[i])
				{
					buttonAccept[i]->winHide(FALSE);
				//Color In the little accepted boxes
					if(slot->isAccepted())
					{
						if(BitIsSet(buttonAccept[i]->winGetStatus(), WIN_STATUS_IMAGE	))
							buttonAccept[i]->winEnable(TRUE);
						else
							GadgetButtonSetEnabledColor(buttonAccept[i], acceptTrueColor );
					}
					else
					{
						if(BitIsSet(buttonAccept[i]->winGetStatus(), WIN_STATUS_IMAGE	))
							buttonAccept[i]->winEnable(FALSE);
						else
							GadgetButtonSetEnabledColor(buttonAccept[i], acceptFalseColor );
					}
				}
			}
			else
			{
				GadgetComboBoxSetSelectedPos(comboPlayer[i], slot->getState(), TRUE);
        if( buttonAccept &&  buttonAccept[i] )
				  buttonAccept[i]->winHide(TRUE);
			}
/*
			if (myGame->getLocalSlotNum() == i && i!=0)
			{
				if (comboPlayer[i])
					comboPlayer[i]->winEnable( TRUE );
			}
			else*/ if (!myGame->amIHost())
			{
				if (comboPlayer[i])
					comboPlayer[i]->winEnable( FALSE );
			}
			//if( i == myGame->getLocalSlotNum())
      if((comboColor[i] != nullptr) && BitIsSet(comboColor[i]->winGetStatus(), WIN_STATUS_ENABLED))
				PopulateColorComboBox(i, comboColor, myGame, myGame->getConstSlot(i)->getPlayerTemplate() == PLAYERTEMPLATE_OBSERVER);
			Int max, idx;
			if (comboColor[i] != nullptr) {
				max = GadgetComboBoxGetLength(comboColor[i]);
				for (idx=0; idx<max; ++idx)
				{
					Int color = (Int)GadgetComboBoxGetItemData(comboColor[i], idx);
					if (color == slot->getColor())
					{
						GadgetComboBoxSetSelectedPos(comboColor[i], idx, TRUE);
						break;
					}
				}
			}

			if (comboTeam[i] != nullptr) {
				max = GadgetComboBoxGetLength(comboTeam[i]);
				for (idx=0; idx<max; ++idx)
				{
					Int team = (Int)GadgetComboBoxGetItemData(comboTeam[i], idx);
					if (team == slot->getTeamNumber())
					{
						GadgetComboBoxSetSelectedPos(comboTeam[i], idx, TRUE);
						break;
					}
				}
			}

			if (comboPlayerTemplate[i] != nullptr) {
				max = GadgetComboBoxGetLength(comboPlayerTemplate[i]);
				for (idx=0; idx<max; ++idx)
				{
					Int playerTemplate = (Int)GadgetComboBoxGetItemData(comboPlayerTemplate[i], idx);
					if (playerTemplate == slot->getPlayerTemplate())
					{
						GadgetComboBoxSetSelectedPos(comboPlayerTemplate[i], idx, TRUE);
						break;
					}
				}
			}
		}
	}
}

// -----------------------------------------------------------------------------
