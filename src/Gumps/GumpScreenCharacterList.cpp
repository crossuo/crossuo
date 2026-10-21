// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GumpScreenCharacterList.h"
#include <SDL_timer.h>
#include <cstdint>
#include "../Config.h"
#include "../ToolTip.h"
#include "../SelectedObject.h"
#include "../CharacterList.h"
#include "../Managers/ClilocManager.h"
#include "../Managers/ConfigManager.h"
#include "../ScreenStages/CharacterListScreen.h"
#include "xuocore/enumlist.h"

enum
{
    ID_CS_NONE,

    ID_CS_QUIT,
    ID_CS_ARROW_PREV,
    ID_CS_ARROW_NEXT,
    ID_CS_NEW,
    ID_CS_DELETE,

    //1.25.32 (1998) character-select buttons; kept before ID_CS_CHARACTERS
    //so the per-slot ids (ID_CS_CHARACTERS + i) stay clear of these values
    //in the tooltip and double-click lookups
    ID_CS_1998_NEW,
    ID_CS_1998_LOGOUT,
    ID_CS_1998_PLAY,
    ID_CS_1998_DELETE,
    ID_CS_1998_PASSWORD,

    ID_CS_CHARACTERS,

    ID_CS_COUNT,
};

CGumpScreenCharacterList::CGumpScreenCharacterList()
    : CGump(GT_NONE, 0, 0, 0)
{
    NoMove = true;
    NoClose = true;
}

void CGumpScreenCharacterList::UpdateContent()
{
    Clear();

    const int count = g_CharacterList.Count;

    // The 1.25.32 (1998) character-select screen, reproduced with the gump
    // art ids and pixel positions of the original client's Login gump: the
    // wood backdrop, the character panel with its five name-slot boxes and
    // the NEW/LOGOUT/PLAY/DELETE/PASSWORD buttons.
    if (g_Config.ClientVersion < CV_200)
    {
        //fullscreen wood backdrop (640x480): stays behind every 1998
        //login-flow screen (the original never cleared the framebuffer
        //between the screens)
        Add(new CGUIGumppic(0x3E8, 0, 0));

        //character-select panel (384x266)
        Add(new CGUIGumppic(0x44C, 124, 104));

        //the five name-slot boxes; the art trio per slot holds brightness
        //variants of the same box: base / unfocused / focused. The selected
        //row is the focused one; empty and named slots look identical - the
        //names are plain text over the box inset (4,2) - the original wrote
        //them into read-only entry controls.
        static const uint16_t slotArt[5][3] = {
            { 0x045C, 0x045D, 0x045E },
            { 0x045F, 0x0460, 0x0461 },
            { 0x0462, 0x0463, 0x0464 },
            { 0x0465, 0x0466, 0x0467 },
            { 0x0468, 0x0469, 0x046A },
        };
        static const int slotPos[5][2] = {
            { 205, 181 }, { 217, 200 }, { 224, 218 }, { 216, 238 }, { 202, 257 },
        };
        static const int slotSize[5][2] = {
            { 222, 15 }, { 197, 15 }, { 184, 16 }, { 199, 15 }, { 228, 16 },
        };
        for (int i = 0; i < 5; i++)
        {
            //slot art: the selected slot is drawn focused (index 2); every
            //other slot - empty or named - uses the unfocused variant (index
            //1). The first art of each trio (base, index 0) is unused here.
            int art = slotArt[i][2];
            if (g_CharacterList.Selected != i)
            {
                art = slotArt[i][1];
            }
            const bool hasName = (i < count && !g_CharacterList.GetName(i).empty());
            Add(new CGUIGumppic(art, slotPos[i][0], slotPos[i][1]));

            if (hasName)
            {
                auto *name = new CGUIText(0x0386, slotPos[i][0] + 4, slotPos[i][1] + 2);
                name->CreateTextureA(1, g_CharacterList.GetName(i));
                Add(name);
            }

            //invisible click area selecting this character (covers the box)
            Add(new CGUIHitBox(
                ID_CS_CHARACTERS + i,
                slotPos[i][0],
                slotPos[i][1],
                slotSize[i][0],
                slotSize[i][1],
                true));
        }

        // The five original buttons (art up/down/pressed). The whole list is
        // rebuilt on selection (see the slot hit box handling below), so no
        // separate art state machine is needed here.
        if (!g_CharacterList.SingleSlot)
        {
            Add(new CGUIButton(ID_CS_1998_NEW, 0x0453, 0x0454, 0x0455, 187, 134));
        }
        Add(new CGUIButton(ID_CS_1998_LOGOUT, 0x0456, 0x0457, 0x0458, 284, 117));
        Add(new CGUIButton(ID_CS_1998_PLAY, 0x0459, 0x045A, 0x045B, 369, 134));
        Add(new CGUIButton(ID_CS_1998_DELETE, 0x044D, 0x044E, 0x044F, 181, 287));
        //no-op in crossuo: the change-password flow is not implemented
        Add(new CGUIButton(ID_CS_1998_PASSWORD, 0x0450, 0x0451, 0x0452, 370, 287));

        return;
    }

    const bool testField = (g_Config.ClientVersion >= CV_305D);
    int posInList = 0;
    int yOffset = 150;
    int yBonus = 0;
    int listTitleY = 106;
    if (g_Config.ClientVersion >= CV_6040)
    {
        listTitleY = 96;
        yOffset = 125;
        yBonus = 45;
    }

    Add(new CGUIGumppicTiled(0x0E14, 0, 0, 640, 480));
    Add(new CGUIGumppic(0x157C, 0, 0));
    Add(new CGUIGumppic(0x15A0, 0, 4));
    Add(new CGUIButton(ID_CS_QUIT, 0x1589, 0x158A, 0x158B, 555, 4));
    Add(new CGUIResizepic(0, 0x0A28, 160, 70, 408, 343 + yBonus));

    auto *obj = new CGUIText(0x0386, 267, listTitleY);
    obj->CreateTextureA(
        2, g_ClilocManager.Cliloc(g_Language)->GetA(3000050, false, "Character Selection"));
    Add(obj);

    for (int i = 0; i < count; i++)
    {
        const auto valid = !g_CharacterList.GetName(i).empty();
        if (!testField || i == 0 || valid)
        {
            const auto id = ID_CS_CHARACTERS + i;
            Add(new CGUIResizepic(id, 0x0BB8, 224, yOffset + (posInList * 40), 280, 30));
            if (valid)
            {
                auto *entry = (CGUITextEntry *)Add(new CGUITextEntry(
                    id,
                    0x034F,
                    0x03E3,
                    0x0021,
                    229,
                    yOffset + (posInList * 40),
                    270,
                    false,
                    5,
                    TS_CENTER,
                    0,
                    32));
                entry->m_Entry.Width = 270;
                entry->m_Entry.SetTextA(g_CharacterList.GetName(i));
                entry->CheckOnSerial = true;
                entry->ReadOnly = true;
                entry->Focused = (g_CharacterList.Selected == i);
            }
            posInList++;
        }
    }

    if (!g_CharacterList.SingleSlot)
    {
        Add(new CGUIButton(ID_CS_NEW, 0x159D, 0x159E, 0x159F, 224, 350 + yBonus));
    }
    Add(new CGUIButton(ID_CS_DELETE, 0x159A, 0x159B, 0x159C, 442, 350 + yBonus));
    Add(new CGUIButton(ID_CS_ARROW_PREV, 0x15A1, 0x15A2, 0x15A3, 586, 445));
    Add(new CGUIButton(ID_CS_ARROW_NEXT, 0x15A4, 0x15A5, 0x15A6, 610, 445));
}

void CGumpScreenCharacterList::InitToolTip()
{
    if (!g_ConfigManager.UseToolTips)
    {
        return;
    }

    const uint32_t id = g_SelectedObject.Serial;
    switch (id)
    {
        case ID_CS_QUIT:
        {
            g_ToolTip.Set(L"Quit Ultima Online", 80);
            break;
        }
        case ID_CS_ARROW_NEXT:
        {
            g_ToolTip.Set(L"Play UO with selected character");
            break;
        }
        case ID_CS_ARROW_PREV:
        {
            g_ToolTip.Set(L"Back to shard selection screen");
            break;
        }
        case ID_CS_NEW:
        {
            g_ToolTip.Set(L"Create new character", 150);
            break;
        }
        case ID_CS_DELETE:
        {
            g_ToolTip.Set(L"Delete this character", 150);
            break;
        }
        case ID_CS_1998_PLAY:
        {
            g_ToolTip.Set(L"Play UO with selected character", 150);
            break;
        }
        case ID_CS_1998_NEW:
        {
            g_ToolTip.Set(L"Create new character", 150);
            break;
        }
        case ID_CS_1998_DELETE:
        {
            g_ToolTip.Set(L"Delete this character", 150);
            break;
        }
        case ID_CS_1998_LOGOUT:
        {
            g_ToolTip.Set(L"Quit Ultima Online", 80);
            break;
        }
        default:
            break;
    }

    if (id >= ID_CS_CHARACTERS)
    {
        const bool testField = (g_Config.ClientVersion >= CV_305D);
        for (int i = 0; i < g_CharacterList.Count; i++)
        {
            if (!testField || i == 0 || !g_CharacterList.GetName(i).empty())
            {
                if (id == (ID_CS_CHARACTERS + i))
                {
                    g_ToolTip.Set(L"Click here to select this character", 150);
                    break;
                }
            }
        }
    }
}

void CGumpScreenCharacterList::GUMP_BUTTON_EVENT_C
{
    if (serial == ID_CS_QUIT)
    {
        g_CharacterListScreen.CreateSmoothAction(CCharacterListScreen::Quit);
    }
    else if (serial == ID_CS_ARROW_PREV)
    {
        g_CharacterListScreen.CreateSmoothAction(CCharacterListScreen::Connect);
    }
    else if (serial == ID_CS_ARROW_NEXT)
    {
        g_CharacterListScreen.CreateSmoothAction(CCharacterListScreen::SelectCharacter);
    }
    else if (serial == ID_CS_NEW)
    {
        g_CharacterListScreen.CreateSmoothAction(CCharacterListScreen::GotoScreenProfession);
    }
    else if (serial == ID_CS_DELETE)
    {
        g_CharacterListScreen.CreateSmoothAction(CCharacterListScreen::GotoScreenDelete);
    }
    else if (serial == ID_CS_1998_NEW)
    {
        g_CharacterListScreen.CreateSmoothAction(CCharacterListScreen::GotoScreenProfession);
    }
    else if (serial == ID_CS_1998_LOGOUT)
    {
        //Main Menu: notify the shard, disconnect and return to the title
        //screen - the original sent the logout packet, dropped the
        //connection and rebuilt its main menu
        g_CharacterListScreen.CreateSmoothAction(CCharacterListScreen::LogOut);
    }
    else if (serial == ID_CS_1998_PLAY)
    {
        g_CharacterListScreen.CreateSmoothAction(CCharacterListScreen::SelectCharacter);
    }
    else if (serial == ID_CS_1998_DELETE)
    {
        //opens the delete/password screen (GotoScreenDelete), but only if a
        //character is selected (has a name); that screen asks for the account
        //password again and, on confirm, sends the delete-character packet
        //and returns to the character list
        g_CharacterListScreen.CreateSmoothAction(CCharacterListScreen::GotoScreenDelete);
    }
    //ID_CS_1998_PASSWORD: intentionally inert (no change-password flow)
    else if (serial >= ID_CS_CHARACTERS && serial < (ID_CS_CHARACTERS + g_CharacterList.Count))
    {
        //1998 slot boxes: a click selects the character; the slot art state
        //is rebuilt from g_CharacterList.Selected on the content refresh
        g_CharacterList.Selected = serial - ID_CS_CHARACTERS;
        WantUpdateContent = true;
        WantRedraw = true;
    }
}

void CGumpScreenCharacterList::GUMP_TEXT_ENTRY_EVENT_C
{
    for (int i = 0; i < g_CharacterList.Count; i++)
    {
        if (serial == (ID_CS_CHARACTERS + i))
        {
            g_CharacterList.Selected = i;
            for (CBaseGUI *item = (CBaseGUI *)m_Items; item != nullptr; item = (CBaseGUI *)item->m_Next)
            {
                if (item->Type == GOT_TEXTENTRY)
                {
                    ((CGUITextEntry *)item)->Focused = item->Serial == serial;
                }
            }
            break;
        }
    }
}

bool CGumpScreenCharacterList::OnLeftMouseButtonDoubleClick()
{
    for (int i = 0; i < g_CharacterList.Count; i++)
    {
        if (g_SelectedObject.Serial == (ID_CS_CHARACTERS + i))
        {
            if (g_CharacterList.GetName(i).empty())
            {
                g_CharacterListScreen.CreateSmoothAction(
                    CCharacterListScreen::GotoScreenProfession);
            }
            else if (g_CharacterList.Selected == i)
            {
                g_CharacterListScreen.CreateSmoothAction(CCharacterListScreen::SelectCharacter);
                SDL_Delay(200);
            }

            return true;
        }
    }

    return false;
}
