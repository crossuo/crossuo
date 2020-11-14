// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GumpDrag.h"
#include "../CrossUO.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/GumpManager.h"
#include "../GameObjects/GameWorld.h"
#include "../GameObjects/ObjectOnCursor.h"
#include "../TextEngine/GameConsole.h"

enum
{
    ID_GD_NONE,

    ID_GD_SCROLL,
    ID_GD_OKAY,
    ID_GD_TEXT_FIELD,

    ID_GD_COUNT,
};

CGumpDrag::CGumpDrag(uint32_t serial, short x, short y)
    : CGump(GT_DRAG, serial, x, y)
{
}

CGumpDrag::~CGumpDrag()
{
}

void CGumpDrag::UpdateContent()
{
    Clear();

    Add(new CGUIGumppic(0x085C, 0, 0));

    int count = 0;
    CGameItem *selobj = g_World->FindWorldItem(Serial);

    if (selobj != nullptr)
    {
        count = selobj->Count;
    }

    m_Slider = (CGUISlider *)Add(new CGUISlider(
        ID_GD_SCROLL, 0x0845, 0x0846, 0x0846, 0, false, false, 29, 16, 105, 0, count, count));

    CGump *gumpEntry = g_GumpManager.GetTextEntryOwner();

    if (gumpEntry != nullptr)
    {
        gumpEntry->WantRedraw = true;
    }

    Add(new CGUIHitBox(ID_GD_TEXT_FIELD, 28, 40, 60, 16));

    m_Entry = (CGUITextEntry *)Add(new CGUITextEntry(
        ID_GD_TEXT_FIELD, 0x0386, 0x0386, 0x0386, 29, 42, 0, false, 1, TS_LEFT, 0, count + 1));
    m_Entry->CheckOnSerial = true;
    g_EntryPointer = &m_Entry->m_Entry;
    g_EntryPointer->NumberOnly = true;
    g_EntryPointer->SetTextW(wstr_from(count));

    Add(new CGUIButton(ID_GD_OKAY, 0x081A, 0x081C, 0x081B, 102, 37));
}

void CGumpDrag::GUMP_BUTTON_EVENT_C
{
    if (serial == ID_GD_OKAY)
    { //Button Okay
        OnOkayPressed();
    }
}

void CGumpDrag::GUMP_SLIDER_CLICK_EVENT_C
{
    OnSliderMove(serial);
}

void CGumpDrag::GUMP_SLIDER_MOVE_EVENT_C
{
    if (m_StartText)
    {
        m_StartText = false;
    }

    if (m_Entry != nullptr)
    {
        m_Entry->m_Entry.SetTextW(wstr_from(m_Slider->Value));
    }
}

void CGumpDrag::OnOkayPressed()
{
    if (!g_ObjectInHand.Enabled)
    {
        RemoveMark = true;

        if (m_Slider->Value != 0)
        {
            CGameItem *obj = g_World->FindWorldItem(Serial);

            if (obj != nullptr)
            {
                g_Game.PickupItem(obj, m_Slider->Value);
            }
        }
    }
}

void CGumpDrag::OnTextInput(const TextEvent &ev)
{
    const auto ch = EvChar(ev);
    if (ch >= '0' && ch <= '9')
    {
        if (m_StartText)
        {
            g_EntryPointer->Clear();
            m_StartText = false;
        }

        g_EntryPointer->Insert(ch);

        int val = 0;
        if (g_EntryPointer->Length() != 0u)
        {
            val = str_to_int(g_EntryPointer->GetTextA());
        }

        m_Slider->Value = val;
        m_Slider->CalculateOffset();

        WantRedraw = true;
    }
}

void CGumpDrag::OnKeyDown(const KeyEvent &ev)
{
    CGameItem *item = g_World->FindWorldItem(Serial);
    if (item == nullptr)
    {
        return;
    }

    auto key = EvKey(ev);
    switch (key)
    {
        case KEY_RETURN:
        case KEY_RETURN2:
        {
            OnOkayPressed();
            if (g_ConfigManager.GetConsoleNeedEnter())
            {
                g_EntryPointer = nullptr;
            }
            else
            {
                g_EntryPointer = &g_GameConsole;
            }
            break;
        }
        case KEY_HOME:
        case KEY_END:
        case KEY_LEFT:
        case KEY_RIGHT:
        {
            g_EntryPointer->OnKey(this, key);

            if (m_StartText)
            {
                m_StartText = false;
            }

            WantRedraw = true;
            break;
        }
        case KEY_DELETE:
        case KEY_BACK:
        {
            g_EntryPointer->OnKey(this, key);

            if (m_StartText)
            {
                m_StartText = false;
            }

            int val = 0;

            if (g_EntryPointer->Length() != 0u)
            {
                val = str_to_int(g_EntryPointer->GetTextA());
            }

            m_Slider->Value = val;
            m_Slider->CalculateOffset();

            WantRedraw = true;
            break;
        }
        default:
            break;
    }
}
