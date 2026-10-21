// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <common/str.h>
#include <vector>
#include "GumpChat.h"
#include "../PressedObject.h"
#include "../Managers/ChatManager.h"
#include "../Managers/FontsManager.h"
#include "../Managers/GumpManager.h"
#include "../Network/Packets.h"
#include "../GUI/GUIAlphaBlending.h"
#include "../GUI/GUIButton.h"
#include "../GUI/GUIHitBox.h"
#include "../GUI/GUIHTMLGump.h"
#include "../GUI/GUIHTMLHitBox.h"
#include "../GUI/GUIHTMLText.h"
#include "../GUI/GUIResizepic.h"
#include "../GUI/GUIText.h"
#include "xuocore/enumlist.h"

CGumpChat::CGumpChat(int x, int y)
    : CGump(GT_CHAT, 0, x, y)
{
}

CGumpChat::~CGumpChat()
{
    if (g_EntryPointer != nullptr && ((m_Entry != nullptr && g_EntryPointer == &m_Entry->m_Entry) ||
                                         (m_MessageEntry != nullptr &&
                                             g_EntryPointer == &m_MessageEntry->m_Entry)))
    {
        g_EntryPointer = nullptr;
    }
}

void CGumpChat::UpdateContent()
{
    //Preserve the transient state across rebuilds and release the entry focus
    //since the entry control is about to be destroyed
    if (g_EntryPointer != nullptr && m_Entry != nullptr && g_EntryPointer == &m_Entry->m_Entry)
    {
        UpdateCreationBoxText();
        g_EntryPointer = nullptr;
    }
    if (g_EntryPointer != nullptr && m_MessageEntry != nullptr &&
        g_EntryPointer == &m_MessageEntry->m_Entry)
    {
        m_MessageText = m_MessageEntry->m_Entry.GetTextA();
        g_EntryPointer = nullptr;
    }
    const bool creationBoxOpen = m_CreationBoxOpen;
    int selected = m_SelectedChannel;
    m_Entry = nullptr;
    m_MessageEntry = nullptr;

    Clear();

    Add(new CGUIResizepic(0, 0x0A28, 0, 0, GUMP_WIDTH, GUMP_HEIGHT));

    //"Channels" header, centered horizontally
    CGUIText *header = (CGUIText *)Add(new CGUIText(0x0386, 0, 25));
    header->CreateTextureA(2, "Channels");
    header->SetX((GUMP_WIDTH - g_FontManager.GetWidthA(2, "Channels")) / 2);

    //Bordered list frame with a translucent backing
    Add(new CGUIColoredPolygone(0, 0, 61, 62, 228, 206, 0xFF383838));
    Add(new CGUIAlphaBlending(true, 0.5f));
    Add(new CGUIColoredPolygone(0, 0, 64, 65, 220, 200, 0xFF282828));
    Add(new CGUIAlphaBlending(false, 1.0f));

    //Scrollable channel list
    CGUIHTMLGump *htmlGump =
        (CGUIHTMLGump *)Add(new CGUIHTMLGump(ID_GC_HTML_GUMP, 0, 64, 65, 220, 200, false, true));

    const std::vector<CChatChannel> &channels = g_ChatManager.Channels;
    if (selected >= (int)channels.size())
    {
        selected = -1;
    }
    m_SelectedChannel = selected;

    const int rowHeight = 16;
    int y = 2;
    for (int i = 0; i < (int)channels.size(); i++)
    {
        CGUIHTMLText *label = (CGUIHTMLText *)htmlGump->Add(
            new CGUIHTMLText(0, 3, selected == i ? 0x22 : 0x49, 3, y, 195));
        label->Text = wstr_from(channels[i].Name);
        label->Create(false);
        htmlGump->Add(new CGUIHTMLHitBox(htmlGump, ID_GC_CHANNEL + i, 0, y, 195, rowHeight, true));
        y += rowHeight;
    }
    htmlGump->CalculateDataSize();

    //"Your current channel" caption and name
    CGUIText *caption = (CGUIText *)Add(new CGUIText(0x0386, 0, 275));
    caption->CreateTextureA(2, "Your current channel");
    caption->SetX((GUMP_WIDTH - g_FontManager.GetWidthA(2, "Your current channel")) / 2);

    const astr_t currentChannel = g_ChatManager.CurrentChannelName;
    if (!currentChannel.empty())
    {
        CGUIText *name = (CGUIText *)Add(new CGUIText(0x0386, 0, 300));
        name->CreateTextureA(2, currentChannel);
        name->SetX((GUMP_WIDTH - g_FontManager.GetWidthA(2, currentChannel)) / 2);
    }

    //Join / Leave / Create buttons with their labels
    Add(new CGUIButton(ID_GC_JOIN, 0x0845, 0x0846, 0x0845, 48, 342));
    Add(new CGUIButton(ID_GC_LEAVE, 0x0845, 0x0846, 0x0845, 123, 342));
    Add(new CGUIButton(ID_GC_CREATE, 0x0845, 0x0846, 0x0845, 216, 342));

    static const struct
    {
        int x;
        const char *text;
    } labels[] = { { 65, "Join" }, { 140, "Leave" }, { 233, "Create" } };
    for (const auto &label : labels)
    {
        CGUIText *text = (CGUIText *)Add(new CGUIText(0x0386, label.x, 337));
        text->CreateTextureA(2, label.text);
    }

    //Conference speech input: Enter sends the text to the current channel
    Add(new CGUIHitBox(ID_GC_MSG_ENTRY, 10, 366, GUMP_WIDTH - 20, 18));
    m_MessageEntry = (CGUITextEntry *)Add(new CGUITextEntry(
        ID_GC_MSG_ENTRY, 0, 0, 0, 10, 366, GUMP_WIDTH - 20, false, 0));
    m_MessageEntry->m_Entry.SetTextA(m_MessageText);

    if (creationBoxOpen)
    {
        //Channel creation panel, centered on the gump
        const int boxX = (GUMP_WIDTH - 200) / 2;
        const int boxY = (GUMP_HEIGHT - 60) / 2;
        const int border = 4;

        Add(new CGUIAlphaBlending(true, 0.5f));
        Add(new CGUIColoredPolygone(0, 0, boxX, boxY, 200, 60, 0xFF000001));
        Add(new CGUIAlphaBlending(false, 1.0f));
        Add(new CGUIColoredPolygone(
            0, 0, boxX + border, boxY + border, 200 - border * 2, 60 - border * 2, 0xFF383838));

        CGUIText *title = (CGUIText *)Add(new CGUIText(0x23, boxX + 6, boxY + 6));
        title->CreateTextureW(1, wstr_from("Create a channel"));

        CGUIText *nameLabel = (CGUIText *)Add(new CGUIText(0x23, boxX + 6, boxY + 28));
        nameLabel->CreateTextureW(1, wstr_from("Name"));

        const int entryX = boxX + 50;
        Add(new CGUIHitBox(ID_GC_CB_ENTRY, entryX, boxY + 26, boxX + 200 - border - entryX, 20));
        m_Entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_GC_CB_ENTRY,
            0,
            0,
            0,
            entryX,
            boxY + 28,
            boxX + 200 - border - entryX,
            false,
            0));
        m_Entry->CheckOnSerial = true;
        m_Entry->m_Entry.SetTextA(m_EntryText);

        Add(new CGUIButton(
            ID_GC_CB_CLOSE, 0x0A94, 0x0A95, 0x0A94, boxX + 200 - 19 - 4, boxY + 60 - 19 - 4));
        Add(new CGUIButton(
            ID_GC_CB_OKAY, 0x0A9A, 0x0A9B, 0x0A9A, boxX + 200 - 19 * 2 - 4, boxY + 60 - 19 - 4));
    }

    WantRedraw = true;
}

void CGumpChat::UpdateCreationBoxText()
{
    if (m_Entry != nullptr)
    {
        m_EntryText = m_Entry->m_Entry.GetTextA();
    }
}

void CGumpChat::GUMP_BUTTON_EVENT_C
{
    switch (serial)
    {
        case ID_GC_JOIN:
        {
            if (m_SelectedChannel >= 0 && m_SelectedChannel < (int)g_ChatManager.Channels.size())
            {
                CPacketChatJoin(wstr_from(g_ChatManager.Channels[m_SelectedChannel].Name)).Send();
            }
            break;
        }
        case ID_GC_LEAVE:
        {
            CPacketChatLeaveChannel().Send();
            break;
        }
        case ID_GC_CREATE:
        {
            m_CreationBoxOpen = true;
            WantUpdateContent = true;
            break;
        }
        case ID_GC_CB_OKAY:
        {
            UpdateCreationBoxText();
            const astr_t name = str_trim(m_EntryText);
            if (!name.empty())
            {
                CPacketChatCreateChannel(wstr_from(name)).Send();
            }
            m_CreationBoxOpen = false;
            m_EntryText = {};
            WantUpdateContent = true;
            break;
        }
        case ID_GC_CB_CLOSE:
        {
            m_CreationBoxOpen = false;
            m_EntryText = {};
            WantUpdateContent = true;
            break;
        }
        default:
        {
            if (serial >= ID_GC_CHANNEL)
            {
                //First click selects the row, a second click on the selected
                //row joins it (the Join button always works as well)
                const int index = serial - ID_GC_CHANNEL;
                if (index == m_SelectedChannel)
                {
                    OnButton(ID_GC_JOIN);
                }
                else
                {
                    m_SelectedChannel = index;
                    WantUpdateContent = true;
                }
            }
            break;
        }
    }
}

void CGumpChat::OnLeftMouseButtonUp()
{
    const bool creationFocused = FocusedEntryIs(ID_GC_CB_ENTRY);
    const bool messageFocused = FocusedEntryIs(ID_GC_MSG_ENTRY);

    CGump::OnLeftMouseButtonUp();

    //Release the text entry focus only when the click landed outside of the
    //entry (the base dispatch above is what focuses it on an inside click)
    if (creationFocused && m_Entry != nullptr && !m_Entry->EntryPointerHere())
    {
        UpdateCreationBoxText();
        g_EntryPointer = nullptr;
        WantRedraw = true;
    }
    else if (messageFocused && m_MessageEntry != nullptr && !m_MessageEntry->EntryPointerHere())
    {
        m_MessageText = m_MessageEntry->m_Entry.GetTextA();
        g_EntryPointer = nullptr;
        WantRedraw = true;
    }
}

bool CGumpChat::FocusedEntryIs(int which) const
{
    if (g_EntryPointer == nullptr)
    {
        return false;
    }
    if (which == ID_GC_CB_ENTRY)
    {
        return m_Entry != nullptr && g_EntryPointer == &m_Entry->m_Entry;
    }
    return m_MessageEntry != nullptr && g_EntryPointer == &m_MessageEntry->m_Entry;
}

void CGumpChat::SendCurrentMessage()
{
    if (m_MessageEntry == nullptr)
    {
        return;
    }

    const astr_t text = str_trim(m_MessageEntry->m_Entry.GetTextA());
    if (text.empty())
    {
        return;
    }

    CPacketChatMessage(wstr_from(text)).Send();
    m_MessageEntry->m_Entry.SetTextA("");
    m_MessageText = {};
    WantRedraw = true;
}

void CGumpChat::DebugSendMessage(const astr_t &text)
{
    Info(Client, "CHATTEST: send message '%s'", text.c_str());
    if (m_MessageEntry != nullptr)
    {
        m_MessageEntry->m_Entry.SetTextA(text);
        SendCurrentMessage();
    }
}

void CGumpChat::OnTextInput(const TextEvent &ev)
{
    const bool creationFocused = FocusedEntryIs(ID_GC_CB_ENTRY);
    const bool messageFocused = FocusedEntryIs(ID_GC_MSG_ENTRY);
    if (!creationFocused && !messageFocused)
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

void CGumpChat::OnKeyDown(const KeyEvent &ev)
{
    const bool creationFocused = FocusedEntryIs(ID_GC_CB_ENTRY);
    const bool messageFocused = FocusedEntryIs(ID_GC_MSG_ENTRY);
    if (!creationFocused && !messageFocused)
    {
        return;
    }

    const auto key = EvKey(ev);
    switch (key)
    {
        case KEY_RETURN:
        case KEY_RETURN2:
        {
            if (messageFocused)
            {
                SendCurrentMessage();
            }
            else
            {
                OnButton(ID_GC_CB_OKAY);
            }
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

void CGumpChat::DebugClick(int id)
{
    Info(Client, "CHATTEST: gump click id=%d", id);
    OnButton(id);
}

void CGumpChat::DebugCreateChannel(const astr_t &name)
{
    Info(Client, "CHATTEST: create channel '%s'", name.c_str());
    if (m_Entry != nullptr)
    {
        m_Entry->m_Entry.SetTextA(name);
        m_EntryText = name;
    }
    OnButton(ID_GC_CB_OKAY);
}
