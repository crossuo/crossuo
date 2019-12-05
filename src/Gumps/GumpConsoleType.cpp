// MIT License
// Copyright (C) August 2016 Hotride

#include <common/str.h>
#include "GumpConsoleType.h"
#include "../ToolTip.h"
#include "../SelectedObject.h"
#include "../Managers/ConfigManager.h"
#include "../ScreenStages/GameScreen.h"
#include "../TextEngine/GameConsole.h"

CGumpConsoleType *g_GumpConsoleType = nullptr;

enum
{
    ID_GCT_NONE,

    ID_GCT_NORMAL,
    ID_GCT_YELL,
    ID_GCT_WHISPER,
    ID_GCT_EMOTE,
    ID_GCT_COMMAND,
    ID_GCT_BROADCAST,
    ID_GCT_PARTY,
    ID_GCT_MINIMIZE,
    ID_GCT_SHOW_FULL_TEXT,

    ID_GCT_COUNT,
};

CGumpConsoleType::CGumpConsoleType(bool minimized, bool showFullText)
    : CGump(GT_CONSOLE_TYPE, 0, 0, 0)
    , m_ShowFullText(showFullText)
{
    Minimized = minimized;
    NoMove = true;
    g_GumpConsoleType = this;
}

CGumpConsoleType::~CGumpConsoleType()
{
    g_GumpConsoleType = nullptr;
}

bool CGumpConsoleType::CanBeDisplayed()
{
    return g_ConfigManager.ShowDefaultConsoleEntryMode;
}

void CGumpConsoleType::SetShowFullText(bool val)
{
    m_ShowFullText = val;
    WantUpdateContent = true;
}

bool CGumpConsoleType::ConsoleIsEmpty()
{
    bool result = (g_GameConsole.Length() == 0);

    switch (m_SelectedType)
    {
        case GCTT_YELL:
        case GCTT_WHISPER:
        case GCTT_EMOTE:
        case GCTT_C:
        case GCTT_BROADCAST:
        case GCTT_PARTY:
        {
            result = (g_ConsolePrefix[m_SelectedType] == g_GameConsole.Data());
            break;
        }
        default:
            break;
    }

    return result;
}

void CGumpConsoleType::DeleteConsolePrefix()
{
    static const std::wstring space = L" ";

    switch (m_SelectedType)
    {
        case GCTT_YELL:
        case GCTT_WHISPER:
        case GCTT_EMOTE:
        case GCTT_C:
        case GCTT_BROADCAST:
        case GCTT_PARTY:
        {
            std::wstring str(g_GameConsole.Data());

            if (str.find(g_ConsolePrefix[m_SelectedType]) == 0)
            {
                str.erase(str.begin(), str.begin() + g_ConsolePrefix[m_SelectedType].length());
                g_GameConsole.SetTextW(str);
            }

            break;
        }
        default:
            break;
    }
}

void CGumpConsoleType::SetConsolePrefix()
{
    switch (m_SelectedType)
    {
        case GCTT_YELL:
        case GCTT_WHISPER:
        case GCTT_EMOTE:
        case GCTT_C:
        case GCTT_BROADCAST:
        case GCTT_PARTY:
        {
            std::wstring str(g_GameConsole.Data());
            str = g_ConsolePrefix[m_SelectedType] + str;
            g_GameConsole.SetTextW(str);

            break;
        }
        default:
            break;
    }
}

void CGumpConsoleType::InitToolTip()
{
    uint32_t selected = g_SelectedObject.Serial;

    switch (selected)
    {
        case ID_GCT_NORMAL:
        {
            g_ToolTip.Set(L"Normal entry mode\nno prefix");
            break;
        }
        case ID_GCT_YELL:
        {
            g_ToolTip.Set(L"Yell entry mode\nprefix is '! '");
            break;
        }
        case ID_GCT_WHISPER:
        {
            g_ToolTip.Set(L"Whisper entry mode\nprefix is '; '");
            break;
        }
        case ID_GCT_EMOTE:
        {
            g_ToolTip.Set(L"Emote entry mode\nprefix is ': '");
            break;
        }
        case ID_GCT_COMMAND:
        {
            g_ToolTip.Set(L"Command entry mode\nprefix is '. '");
            break;
        }
        case ID_GCT_BROADCAST:
        {
            g_ToolTip.Set(L"Broadcast entry mode\nprefix is '? '");
            break;
        }
        case ID_GCT_PARTY:
        {
            g_ToolTip.Set(L"Party entry mode\nprefix is '/ '");
            break;
        }
        case ID_GCT_MINIMIZE:
        {
            g_ToolTip.Set(L"Show/hide configuration");
            break;
        }
        case ID_GCT_SHOW_FULL_TEXT:
        {
            g_ToolTip.Set(L"Show full mode names");
            break;
        }
        default:
            break;
    }
}

void CGumpConsoleType::UpdateContent()
{
    Clear();

    CGUIText *obj = (CGUIText *)Add(new CGUIText(0, 14, 0));
    obj->CreateTextureA(3, "Default entry text mode:");

    if (Minimized)
    {
        Add(new CGUIButton(ID_GCT_MINIMIZE, 0x0985, 0x0986, 0x0986, 0, 6));
    }
    else
    {
        Add(new CGUIButton(ID_GCT_MINIMIZE, 0x0983, 0x0984, 0x0984, 0, 6));

        int offsetX = 0;
        int offsetY = obj->m_Texture.Height;

        const char *text[] = { " Normal ",  " Yell ",      " Whisper ", " Emote ",
                               " Command ", " Broadcast ", " Party " };

        int text0Height = 0;

        for (int i = 0; i < 7; i++)
        {
            CGUITextEntry *entry =
                new CGUITextEntry((int)i + 1, 0, 0, 0, offsetX, offsetY, 0, false, 3);
            entry->CheckOnSerial = true;
            entry->ReadOnly = true;
            entry->SetGlobalColor(true, 0xFFFFFFFF, 0xFF267FFF, 0xFF00FF00);
            entry->Focused = (i == m_SelectedType);

            if (m_ShowFullText || (i == 0))
            {
                entry->m_Entry.SetTextA(text[i]);
                entry->m_Entry.CreateTextureA(3, text[i], 0, 0, TS_LEFT, 0);
            }
            else
            {
                std::string str = " ";
                str += ToString(g_ConsolePrefix[i]);

                if (str.length() < 3)
                {
                    str += " ";
                }

                entry->m_Entry.SetTextA(str);
                entry->m_Entry.CreateTextureA(3, str, 0, 0, TS_LEFT, 0);
            }

            if (i == 0)
            {
                text0Height = entry->m_Entry.m_Texture.Height;
            }

            Add(new CGUIColoredPolygone(
                (int)i + 1,
                0,
                offsetX + 1,
                offsetY,
                entry->m_Entry.m_Texture.Width - 2,
                entry->m_Entry.m_Texture.Height,
                0x3F000000));
            Add(entry);

            offsetX += entry->m_Entry.m_Texture.Width;
        }

        offsetY += text0Height + 3;

        CGUICheckbox *checkbox = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_GCT_SHOW_FULL_TEXT, 0x00D2, 0x00D3, 0x00D2, 0, offsetY + 2));
        checkbox->Checked = m_ShowFullText;

        obj = (CGUIText *)Add(new CGUIText(0, 24, offsetY));
        obj->CreateTextureA(3, "Show full mode name");
    }
}

void CGumpConsoleType::CalculateGumpState()
{
    CGump::CalculateGumpState();

    g_GumpMovingOffset.X = 0;
    g_GumpMovingOffset.Y = 0;

    g_GumpTranslate.X = (float)(g_RenderBounds.GameWindowPosX + 2);
    g_GumpTranslate.Y =
        (float)(g_RenderBounds.GameWindowPosY + g_RenderBounds.GameWindowHeight + 2);
}

void CGumpConsoleType::GUMP_BUTTON_EVENT_C
{
    if (serial == ID_GCT_MINIMIZE)
    {
        Minimized = !Minimized;
        WantUpdateContent = true;
    }
    else if (serial == ID_GCT_SHOW_FULL_TEXT)
    {
        m_ShowFullText = !m_ShowFullText;
        WantUpdateContent = true;
    }
}

void CGumpConsoleType::GUMP_CHECKBOX_EVENT_C
{
    if (serial == ID_GCT_SHOW_FULL_TEXT && m_ShowFullText != state)
    {
        m_ShowFullText = state;
        WantUpdateContent = true;
    }
}

void CGumpConsoleType::GUMP_TEXT_ENTRY_EVENT_C
{
    DeleteConsolePrefix();

    m_SelectedType = serial - 1;

    QFOR(item, m_Items, CBaseGUI *)
    {
        if (item->Type == GOT_TEXTENTRY)
        {
            CGUITextEntry *entry = (CGUITextEntry *)item;
            entry->Focused = (item->Serial == serial);
        }
    }

    SetConsolePrefix();

    WantRedraw = true;
}
