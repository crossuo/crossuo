// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GumpChatChooseName.h"
#include "../PressedObject.h"
#include "../Managers/GumpManager.h"
#include "../Network/Packets.h"
#include "../GUI/GUIHitBox.h"
#include "../GUI/GUIText.h"
#include "xuocore/enumlist.h"

CGumpChatChooseName::CGumpChatChooseName(int x, int y)
    : CGump(GT_CHAT_CHOOSE_NAME, 0, x, y)
{
    //A fixed-size dialog, it must not be dragged around
    NoMove = true;
}

CGumpChatChooseName::~CGumpChatChooseName()
{
    if (g_EntryPointer != nullptr && m_Entry != nullptr && g_EntryPointer == &m_Entry->m_Entry)
    {
        g_EntryPointer = nullptr;
    }
}

void CGumpChatChooseName::UpdateContent()
{
    Clear();

    const int WIDTH = 210;
    const int HEIGHT = 330;
    const int BORDER = 4;

    //Translucent background with an approximated border: a dark outer
    //polygone with a lighter fill inset by the border width
    Add(new CGUIAlphaBlending(true, 0.5f));
    Add(new CGUIColoredPolygone(0, 0, 0, 0, WIDTH, HEIGHT, 0xFF000001));
    Add(new CGUIColoredPolygone(
        0, 0, BORDER, BORDER, WIDTH - BORDER * 2, HEIGHT - BORDER * 2, 0xFF383838));
    Add(new CGUIAlphaBlending(false, 1.0f));

    CGUIText *title = (CGUIText *)Add(new CGUIText(0x17, 6, 6));
    title->CreateTextureW(3, wstr_from("Choose a character name:"), 30, WIDTH - 17);

    //Name row: a label and a text entry sharing a 27px-high bordered row
    const int row1Y = 6 + title->GetSize().Height + 2;
    const int rowHeight = 27;
    CGUIText *text = (CGUIText *)Add(new CGUIText(0x033, 6, row1Y));
    text->CreateTextureW(3, wstr_from("Name:"), 30);

    const int entryX = 50;
    Add(new CGUIHitBox(ID_GCCN_ENTRY, entryX, row1Y, WIDTH - BORDER - entryX, rowHeight));
    m_Entry = (CGUITextEntry *)Add(new CGUITextEntry(ID_GCCN_ENTRY, 0, 0, 0,
        entryX,
        row1Y + 2,
        WIDTH - BORDER - entryX,
        false,
        0));
    m_Entry->CheckOnSerial = true;

    //Second bordered row below the name row (separator)
    Add(new CGUIColoredPolygone(
        0, 0, BORDER, row1Y + rowHeight + 2, WIDTH - BORDER * 2, rowHeight, 0xFF282828));

    Add(new CGUIButton(ID_GCCN_CLOSE, 0x0A94, 0x0A95, 0x0A94, WIDTH - 19 - 4, HEIGHT - 19 - 4));
    Add(new CGUIButton(ID_GCCN_OKAY, 0x0A9A, 0x0A9B, 0x0A9A, WIDTH - 19 * 2 - 4, HEIGHT - 19 - 4));

    WantRedraw = true;
}

void CGumpChatChooseName::GUMP_BUTTON_EVENT_C
{
    if (serial == ID_GCCN_OKAY)
    {
        const astr_t name = str_trim(m_Entry->m_Entry.GetTextA());
        if (!name.empty())
        {
            CPacketOpenChat(wstr_from(name)).Send();
        }
        RemoveMark = true;
    }
    else if (serial == ID_GCCN_CLOSE)
    {
        RemoveMark = true;
    }
}

void CGumpChatChooseName::OnLeftMouseButtonUp()
{
    //Release the text entry focus when clicking outside of the hit box
    if (g_EntryPointer != nullptr && m_Entry != nullptr && g_EntryPointer == &m_Entry->m_Entry)
    {
        g_EntryPointer = nullptr;
        WantRedraw = true;
    }

    CGump::OnLeftMouseButtonUp();
}

void CGumpChatChooseName::OnTextInput(const TextEvent &ev)
{
    if (g_EntryPointer == nullptr || m_Entry == nullptr || g_EntryPointer != &m_Entry->m_Entry)
    {
        return;
    }

    const auto ch = EvChar(ev);
    if (ch != '\0' && ch != '\n' && ch != '\r')
    {
        g_EntryPointer->Insert(ch);
        WantRedraw = true;
    }
}

void CGumpChatChooseName::OnKeyDown(const KeyEvent &ev)
{
    if (g_EntryPointer == nullptr || m_Entry == nullptr || g_EntryPointer != &m_Entry->m_Entry)
    {
        return;
    }

    const auto key = EvKey(ev);
    switch (key)
    {
        case KEY_RETURN:
        case KEY_RETURN2:
        {
            OnButton(ID_GCCN_OKAY);
            break;
        }
        case KEY_HOME:
        case KEY_END:
        case KEY_LEFT:
        case KEY_RIGHT:
        case KEY_DELETE:
        case KEY_BACK:
        {
            g_EntryPointer->OnKey(this, key);
            WantRedraw = true;
            break;
        }
        default:
            break;
    }
}
