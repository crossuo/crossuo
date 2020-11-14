// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GumpScreenCharacterList.h"
#include <SDL_timer.h>
#include "../Config.h"
#include "../ToolTip.h"
#include "../SelectedObject.h"
#include "../CharacterList.h"
#include "../Managers/ClilocManager.h"
#include "../Managers/ConfigManager.h"
#include "../ScreenStages/CharacterListScreen.h"

enum
{
    ID_CS_NONE,

    ID_CS_QUIT,
    ID_CS_ARROW_PREV,
    ID_CS_ARROW_NEXT,
    ID_CS_NEW,
    ID_CS_DELETE,
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
}

void CGumpScreenCharacterList::GUMP_TEXT_ENTRY_EVENT_C
{
    for (int i = 0; i < g_CharacterList.Count; i++)
    {
        if (serial == (ID_CS_CHARACTERS + i))
        {
            g_CharacterList.Selected = i;
            QFOR(item, m_Items, CBaseGUI *)
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
