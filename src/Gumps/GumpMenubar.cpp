// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GumpMenubar.h"
#include "../CrossUO.h"
#include "../GameWindow.h"
#include "../Config.h"
#include <cstdint>
#include <vector>
#include "../ToolTip.h"
#include "../SelectedObject.h"
#include "../Managers/ClilocManager.h"
#include "../Network/Packets.h"
#include "xuocore/enumlist.h"
#if defined(XUO_ASSISTANT)
#include <macros/macroexecutor.h> // xuoassist::g_MacroExecutor.RecordHelpRequest (macro tap)
#include "../Assistant/AssistantWindow.h" // AssistantWindow_Toggle (the assistant button)
#endif

enum
{
    ID_GMB_NONE,

    ID_GMB_MINIMIZE,
    ID_GMB_MAP,
    ID_GMB_PAPERDOLL,
    ID_GMB_INVENTORY,
    ID_GMB_JOURNAL,
    ID_GMB_CHAT,
    ID_GMB_HELP,
    ID_GMB_WORLD_MAP,
    ID_GMB_STORE,
    ID_GMB_INFO,
#if defined(XUO_ASSISTANT)
    ID_GMB_ASSISTANT,
#endif

    ID_GMB_COUNT,
};

CGumpMenubar::CGumpMenubar(short x, short y)
    : CGump(GT_MENUBAR, 0, x, y)
{
    RebuildLayout();
}

// builds the whole content: page 1 is the minimized stub, page 2 the bar
// itself. Web mobile lays the entries over as many lines as the canvas is
// wide, so rotation re-runs this through UpdateContent
void CGumpMenubar::RebuildLayout()
{
    const int page = Page;
    Clear();
    Page = page;

    Add(new CGUIPage(1));
    Add(new CGUIResizepic(0, 0x13BE, 0, 0, 30, 27));
    Add(new CGUIButton(ID_GMB_MINIMIZE, 0x15A1, 0x15A1, 0x15A1, 5, 3));
    Add(new CGUIPage(2));
    auto *spr1 = g_Game.ExecuteGump(0x098B);
    auto *spr2 = g_Game.ExecuteGump(0x098D);

    int smallWidth = 50;
    if (spr1 != nullptr)
    {
        smallWidth = spr1->Width;
    }

    int largeWidth = 100;
    if (spr2 != nullptr)
    {
        largeWidth = spr2->Width;
    }

    // the bar is sized from the entries: each button uses its tile art width
    // and the entries are laid out left to right (the reference computes the
    // background width the same way)
    const bool hasUOStore = (g_Config.ProtocolClientVersion >= VERSION(7, 0, 64, 0));

    struct MenuEntry
    {
        uint16_t art;
        int artWidth;
        int id;
        wstr_t text;
    };

    std::vector<MenuEntry> entries = {
        { 0x098B,
          smallWidth,
          ID_GMB_MAP,
          g_ClilocManager.Cliloc(g_Language)->GetW(3000430, false, "Map") },
        { 0x098D,
          largeWidth,
          ID_GMB_PAPERDOLL,
          g_ClilocManager.Cliloc(g_Language)->GetW(3002133, false, "Paperdoll") },
        { 0x098D,
          largeWidth,
          ID_GMB_INVENTORY,
          g_ClilocManager.Cliloc(g_Language)->GetW(3000431, false, "Inventory") },
        { 0x098D,
          largeWidth,
          ID_GMB_JOURNAL,
          g_ClilocManager.Cliloc(g_Language)->GetW(3002135, false, "Journal") },
        { 0x098B, smallWidth, ID_GMB_CHAT, L"Chat" },
        { 0x098B,
          smallWidth,
          ID_GMB_HELP,
          g_ClilocManager.Cliloc(g_Language)->GetW(3000134, false, "Help") },
        { 0x098D, largeWidth, ID_GMB_WORLD_MAP, L"World Map" },
    };
    if (hasUOStore)
    {
        entries.push_back(
            { 0x098D,
              largeWidth,
              ID_GMB_STORE,
              g_ClilocManager.Cliloc(g_Language)->GetW(1158008, false, "Store") });
    }
#if defined(XUO_ASSISTANT)
    if (g_Config.AssistantEnabled)
    {
        entries.push_back({ 0x098B, smallWidth, ID_GMB_ASSISTANT, L"Assistant" });
    }
#endif
    //entries.push_back({ 0x098B, smallWidth, ID_GMB_INFO, L"< ? >" });

    // web mobile only: entries wrap over as many 27px lines as the canvas
    // needs when one line would be wider than it (the bar is unreadable
    // otherwise); everywhere else - including web on a desktop, even with
    // a touchscreen - the single row is kept as authored
    int rowWidth = 0;
#if defined(XUO_EMSC)
    if (WebIsMobile())
    {
        rowWidth = g_GameWindow.GetSize().Width;
        m_BuiltCanvasWidth = rowWidth;
    }
#else
    (void)rowWidth;
#endif

    struct PlacedEntry
    {
        const MenuEntry *entry;
        int x;
        int y;
    };

    std::vector<PlacedEntry> placed;
    int startX = 30;
    int startY = 0;
    int rowEnd = startX;

    for (const MenuEntry &entry : entries)
    {
        if (rowWidth > 0 && startX > 30 && startX + entry.artWidth + 1 > rowWidth)
        {
            startX = 30;
            startY += 27;
        }

        placed.push_back({ &entry, startX, startY });
        startX += entry.artWidth + 1;

        if (startX > rowEnd)
        {
            rowEnd = startX;
        }
    }

    Add(new CGUIResizepic(0, 0x13BE, 0, 0, rowEnd + 1, 27 + startY));
    Add(new CGUIButton(ID_GMB_MINIMIZE, 0x15A4, 0x15A4, 0x15A4, 5, 3));

    for (const PlacedEntry &placedEntry : placed)
    {
        const MenuEntry &entry = *placedEntry.entry;
        Add(new CGUIButton(
            entry.id, entry.art, entry.art, entry.art, placedEntry.x, 1 + placedEntry.y));

        CGUITextEntry *textEntry = (CGUITextEntry *)Add(new CGUITextEntry(
            entry.id,
            0,
            0x0036,
            0x0036,
            placedEntry.x + 2,
            2 + placedEntry.y,
            entry.artWidth,
            true,
            1,
            TS_CENTER));
        textEntry->m_Entry.SetTextW(entry.text);
        textEntry->CheckOnSerial = true;
        textEntry->ReadOnly = true;
        textEntry->FocusedOffsetY = 2;
    }
}

void CGumpMenubar::UpdateContent()
{
#if defined(XUO_EMSC)
    if (WebIsMobile() && g_GameWindow.GetSize().Width != m_BuiltCanvasWidth)
    {
        // the canvas followed the rotated viewport: relayout
        RebuildLayout();
        WantRedraw = true;
    }
#endif
}

CGumpMenubar::~CGumpMenubar()
= default;

void CGumpMenubar::SetOpened(bool val)
{
    m_Opened = val;
    if (val)
    {
        Page = 2;
    }
    else
    {
        Page = 1;
    }
    WantRedraw = true;
}

void CGumpMenubar::InitToolTip()
{
    uint32_t id = g_SelectedObject.Serial;
    if (!Minimized)
    {
        switch (id)
        {
            case ID_GMB_MINIMIZE:
            {
                g_ToolTip.Set(L"Minimize the menubar gump");
                break;
            }
            case ID_GMB_MAP:
            {
                g_ToolTip.Set(L"Open the minimap gump");
                break;
            }
            case ID_GMB_PAPERDOLL:
            {
                g_ToolTip.Set(L"Open the paperdoll gump");
                break;
            }
            case ID_GMB_INVENTORY:
            {
                g_ToolTip.Set(L"Open backpack");
                break;
            }
            case ID_GMB_JOURNAL:
            {
                g_ToolTip.Set(L"Open the journal gump");
                break;
            }
            case ID_GMB_CHAT:
            {
                g_ToolTip.Set(L"Open the chat gump");
                break;
            }
            case ID_GMB_HELP:
            {
                g_ToolTip.Set(L"Open server's help menu gump");
                break;
            }
            case ID_GMB_WORLD_MAP:
            {
                g_ToolTip.Set(L"Open CrossUO's world map gump");
                break;
            }
            case ID_GMB_STORE:
            {
                g_ToolTip.Set(L"Open the UO store");
                break;
            }
            /*case ID_GMB_INFO:
            {
                g_ToolTip.Set(L"(Not implemented)");
                break;
            }*/
#if defined(XUO_ASSISTANT)
            case ID_GMB_ASSISTANT:
            {
                g_ToolTip.Set(L"Toggle the assistant window");
                break;
            }
#endif
            default:
                break;
        }
    }
    else
    {
        g_ToolTip.Set(L"Maximize the menubar gump");
    }
}

void CGumpMenubar::GUMP_BUTTON_EVENT_C
{
    switch (serial)
    {
        case ID_GMB_MINIMIZE:
        {
            m_Opened = !m_Opened;
            Page = 1 + (int)m_Opened;
            WantUpdateContent = true;
            break;
        }
        case ID_GMB_MAP:
        {
            g_Game.OpenMinimap();
            break;
        }
        case ID_GMB_PAPERDOLL:
        {
            g_Game.PaperdollReq(g_PlayerSerial);
            break;
        }
        case ID_GMB_INVENTORY:
        {
            g_Game.OpenBackpack();
            break;
        }
        case ID_GMB_JOURNAL:
        {
            g_Game.OpenJournal();
            break;
        }
        case ID_GMB_CHAT:
        {
            g_Game.OpenChat();
            break;
        }
        case ID_GMB_HELP:
        {
            CPacketHelpRequest().Send();
#if defined(XUO_ASSISTANT)
            xuoassist::g_MacroExecutor.RecordHelpRequest();
#endif
            break;
        }
        case ID_GMB_WORLD_MAP:
        {
            g_Game.OpenWorldMap();
            break;
        }
        case ID_GMB_STORE:
        {
            if (g_Config.ProtocolClientVersion >= VERSION(7, 0, 64, 0))
            {
                CPacketOpenUOStore().Send();
            }
            break;
        }
        case ID_GMB_INFO:
        {
            break;
        }
#if defined(XUO_ASSISTANT)
        case ID_GMB_ASSISTANT:
        {
            AssistantWindow_Toggle();
            break;
        }
#endif
        default:
            break;
    }
}

void CGumpMenubar::GUMP_TEXT_ENTRY_EVENT_C
{
    for (CBaseGUI *item = (CBaseGUI *)m_Items; item != nullptr; item = (CBaseGUI *)item->m_Next)
    {
        if (item->Type == GOT_TEXTENTRY)
        {
            CGUITextEntry *entry = (CGUITextEntry *)item;
            entry->Focused = (entry->Serial == serial);
        }
    }
    OnButton(serial);
}

void CGumpMenubar::OnLeftMouseButtonUp()
{
    CGump::OnLeftMouseButtonUp();
    for (CBaseGUI *item = (CBaseGUI *)m_Items; item != nullptr; item = (CBaseGUI *)item->m_Next)
    {
        if (item->Type == GOT_TEXTENTRY)
        {
            CGUITextEntry *entry = (CGUITextEntry *)item;
            entry->Focused = false;
        }
    }
    WantRedraw = true;
}
