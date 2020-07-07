// MIT License
// Copyright (C) August 2016 Hotride

#include <common/str.h>
#include "GameConsole.h"
#include "../CrossUO.h"
#include "../Party.h"
#include "../Managers/FontsManager.h"
#include "../Managers/ConfigManager.h"
#include "../Network/Packets.h"
#include "../GameObjects/GamePlayer.h"

CGameConsole g_GameConsole;

static const char *s_ConsolePrefix[] = {
    "",   //Normal
    "! ", //Yell
    "; ", //Whisper
    ": ", //Emote
    ".",  //Command
    "?",  //Broadcast
    "/",  //Party
    "\\", //Guild
    "|",  //Alliance
};

static wstr_t GetConsolePrefixAndType(const wstr_t &text, int &member, GAME_CONSOLE_TEXT_TYPE &type)
{
    type = GCTT_NORMAL;
    wstr_t result;

    const auto len = text.size();
    auto astr = str_from(text);
    const char *atext = astr.c_str();
    char aprefix = *atext;
    if (aprefix == s_ConsolePrefix[GCTT_PARTY][0]) // Party
    {
        char *ptr = (char *)atext + 1;
        while (ptr < atext + len && *ptr == ' ')
        {
            ptr++;
        }

        if (ptr < atext + len)
        {
            int i = 0;
            (void)sscanf(ptr, "%i", &i);
            if (i > 0 && i < 11) // Party member
            {
                char pmBuf[50];
                if (g_Party.Member[i - 1].Serial != 0)
                {
                    auto name = g_Party.Member[i - 1].GetName(i);
                    snprintf(pmBuf, sizeof(pmBuf), "Tell [%s]:", name.c_str());
                }
                else
                {
                    snprintf(pmBuf, sizeof(pmBuf), "Tell []:");
                }

                result = wstr_from(pmBuf);
                type = GCTT_PARTY;
                member = i - 1;
            }
        }

        if (type == GCTT_NORMAL && len >= 4 && len <= 8)
        {
            if (strncasecmp(atext, "/add", 4) == 0)
            {
                type = GCTT_PARTY_ADD; // Party add
            }
            else if (strncasecmp(atext, "/quit", 5) == 0 || strncasecmp(atext, "/leave", 6) == 0)
            {
                type = GCTT_PARTY_LEAVE; // Party leave & quit
            }
            else if (strncasecmp(atext, "/accept", 7) == 0)
            {
                type = GCTT_PARTY_ACCEPT; // Party accept
            }
            else if (strncasecmp(atext, "/decline", 8) == 0)
            {
                type = GCTT_PARTY_DECLINE; // Party decline
            }
        }

        if ((type == GCTT_PARTY_ADD || type == GCTT_PARTY_LEAVE || type == GCTT_PARTY_ACCEPT ||
             type == GCTT_PARTY_DECLINE) &&
            (result.length() == 0u))
        {
            result = L"Party:";
        }

        if (type == GCTT_NORMAL && result.length() == 0u)
        {
            result = L"Party:";
            type = GCTT_PARTY;
        }
    }
    else if (aprefix == s_ConsolePrefix[GCTT_YELL][0])
    {
        result = L"Yell:";
        type = GCTT_YELL;
    }
    else if (aprefix == s_ConsolePrefix[GCTT_WHISPER][0])
    {
        result = L"Whisper:";
        type = GCTT_WHISPER;
    }
    else if (aprefix == s_ConsolePrefix[GCTT_EMOTE][0])
    {
        result = L"Emote:";
        type = GCTT_EMOTE;
    }
    else if (
        g_Player->Graphic == 0x03DB && (*atext == '=' || aprefix == s_ConsolePrefix[GCTT_C][0]))
    {
        result = L"Command:";
        type = GCTT_C;
    }
    else if (aprefix == s_ConsolePrefix[GCTT_BROADCAST][0])
    {
        result = L"Broadcast:";
        type = GCTT_BROADCAST;
    }
    else if (aprefix == s_ConsolePrefix[GCTT_GUILD][0])
    {
        result = L"Guild:";
        type = GCTT_GUILD;
    }
    else if (aprefix == s_ConsolePrefix[GCTT_ALLIANCE][0])
    {
        result = L"Alliance:";
        type = GCTT_ALLIANCE;
    }

    return result + L" ";
}

void SendConsoleText(wstr_t text, uint16_t defaultColor)
{
    const size_t len = text.length();
    if (len == 0)
        return;

    SPEECH_TYPE speechType = ST_NORMAL;
    uint16_t sendColor = g_ConfigManager.SpeechColor;
    int offset = 0;
    int member = -1;
    GAME_CONSOLE_TEXT_TYPE type = GCTT_NORMAL;
    (void)GetConsolePrefixAndType(text, member, type);
    if (len == 1 && type != GCTT_NORMAL)
        return;

    if (len > 1)
    {
        offset = (int)strlen(s_ConsolePrefix[type]);
        if ((type != GCTT_NORMAL && len > 2) || type == GCTT_PARTY)
        {
            if (type == GCTT_YELL)
            {
                speechType = ST_YELL;
            }
            else if (type == GCTT_WHISPER)
            {
                speechType = ST_WHISPER;
            }
            else if (type == GCTT_EMOTE)
            {
                text = text.replace(0, offset, L": *").append(L"*");
                speechType = ST_EMOTE;
                sendColor = g_ConfigManager.EmoteColor;
            }
            else if (type == GCTT_GUILD)
            {
                speechType = ST_GUILD_CHAT;
                sendColor = g_ConfigManager.GuildMessageColor;
            }
            else if (type == GCTT_ALLIANCE)
            {
                sendColor = g_ConfigManager.AllianceMessageColor;
                speechType = ST_ALLIANCE_CHAT;
            }
            else if (type == GCTT_PARTY)
            {
                uint32_t serial = 0;
                sendColor = g_ConfigManager.PartyMessageColor;
                if (member != -1)
                {
                    serial = g_Party.Member[member].Serial;
                }

                text.erase(0, offset);
                if (g_Party.Leader != 0)
                {
                    CPacketPartyMessage(text.c_str(), len - offset, serial).Send();
                }
                else
                {
                    astr_t str = "Note to self: " + str_from(text);
                    g_Game.CreateTextMessage(TT_SYSTEM, 0, 3, 0, str);
                }
                return;
            }
            else if (type == GCTT_PARTY_ACCEPT)
            {
                if (g_Party.Inviter != 0 && g_Party.Leader == 0)
                {
                    CPacketPartyAccept(g_Party.Inviter).Send();
                    g_Party.Leader = g_Party.Inviter;
                    g_Party.Inviter = 0;
                }
                else
                {
                    g_Game.CreateTextMessage(
                        TT_SYSTEM, 0, 3, 0, "No one has invited you to be in a party.");
                }
                return;
            }
            else if (type == GCTT_PARTY_DECLINE)
            {
                if (g_Party.Inviter != 0 && g_Party.Leader == 0)
                {
                    CPacketPartyDecline(g_Party.Inviter).Send();
                    g_Party.Leader = 0;
                    g_Party.Inviter = 0;
                }
                else
                {
                    g_Game.CreateTextMessage(
                        TT_SYSTEM, 0, 3, 0, "No one has invited you to be in a party.");
                }
                return;
            }
            else if (type == GCTT_PARTY_ADD)
            {
                if (g_Party.Leader == 0 || g_Party.Leader == g_PlayerSerial)
                {
                    CPacketPartyInviteRequest().Send();
                }
                else
                {
                    g_Game.CreateTextMessage(TT_SYSTEM, 0, 3, 0, "You are not party leader.");
                }
                return;
            }
            else if (type == GCTT_PARTY_LEAVE)
            {
                if (g_Party.Leader != 0)
                {
                    for (int i = 0; i < 10; i++)
                    {
                        if (g_Party.Member[i].Serial != 0)
                        {
                            CPacketPartyRemoveRequest(g_Party.Member[i].Serial).Send();
                        }
                    }
                }
                else
                {
                    g_Game.CreateTextMessage(
                        TT_SYSTEM, 0xFFFFFFFF, 3, 0, "You are not in a party.");
                }
                return;
            }
        }
    }

    if (defaultColor != 0u)
    {
        sendColor = defaultColor;
    }

    text.erase(0, offset);
    CPacketUnicodeSpeechRequest(text, speechType, 3, sendColor, g_Language).Send();
}

CGameConsole::CGameConsole()
    : CEntryText(128, 1000, 1000)
{
}

CGameConsole::~CGameConsole()
{
    if (g_EntryPointer == this)
    {
        g_EntryPointer = nullptr;
    }
}

void CGameConsole::Send()
{
    SendConsoleText(WText);
    m_Type = GCTT_NORMAL;
}

bool CGameConsole::InChat() const
{
    return (m_Type > GCTT_NORMAL);
}

void CGameConsole::DrawW(
    uint8_t font, uint16_t color, int x, int y, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    wstr_t wtext = GetTextW();
    if (wtext.empty())
    {
        m_Type = GCTT_NORMAL;
    }

    if (Changed || Color != color)
    {
        FixMaxWidthW(font);
    }

    int member = 0;
    const auto sysStr = GetConsolePrefixAndType(wtext, member, m_Type);
    const auto offset = strlen(s_ConsolePrefix[m_Type]);
    const bool expand = offset && wtext.size() >= offset &&
                        (char)wtext[offset - 1] == s_ConsolePrefix[m_Type][offset - 1];
    if (!sysStr.empty() && expand)
    {
        wtext = sysStr + &wtext.c_str()[offset]; // remove prefix
    }

    if (Changed || Color != color)
    {
        (void)CheckMaxWidthW(font, wtext);
        CreateTextureW(font, wtext, color, Width, align, flags);
        Changed = false;
        Color = color;
    }

    m_Texture.Draw(x, y);
    if (this == g_EntryPointer)
    {
        if (m_Position != 0)
        {
            wtext.resize(m_Position);
            x += g_FontManager.GetWidthW(font, wtext);
        }
        g_FontManager.DrawW(font, L"_", color, x, y, 30, 0, align, UOFONT_BLACK_BORDER);
    }
}

void CGameConsole::SaveConsoleMessage()
{
    m_ConsoleStack[m_ConsoleStackCount % MAX_CONSOLE_STACK_SIZE] = WText;
    m_ConsoleStackCount++;
    if (m_ConsoleStackCount > 1100)
    {
        m_ConsoleStackCount -= 1000;
    }
    m_ConsoleSelectedIndex = (m_ConsoleStackCount - 1) % MAX_CONSOLE_STACK_SIZE;
    m_PositionChanged = false;
}

bool CGameConsole::Insert(char16_t ch, CGump *gump)
{
    // For Parity with original client, when the first console character
    // is the prefix for Party, Guild or Alliance, they can be automatically
    // replaced between each of them so communication is easier
    auto replacable = [](auto c) -> bool {
        return (char)c == s_ConsolePrefix[GCTT_PARTY][0] ||
               (char)c == s_ConsolePrefix[GCTT_GUILD][0] ||
               (char)c == s_ConsolePrefix[GCTT_ALLIANCE][0];
    };
    if (WText.size() == 1 && replacable(WText[0]) && replacable(ch))
    {
        WText.clear();
    }
    return CEntryText::Insert(ch, gump);
}

void CGameConsole::ChangeConsoleMessage(bool next)
{
    if (m_ConsoleStackCount != 0)
    {
        if (m_PositionChanged)
        {
            if (next)
            {
                m_ConsoleSelectedIndex = (m_ConsoleSelectedIndex + 1) % MAX_CONSOLE_STACK_SIZE;
                if (m_ConsoleSelectedIndex >= m_ConsoleStackCount)
                {
                    m_ConsoleSelectedIndex = 0;
                }
            }
            else
            {
                m_ConsoleSelectedIndex--;
                if (m_ConsoleSelectedIndex < 0)
                {
                    m_ConsoleSelectedIndex = (m_ConsoleStackCount - 1) % MAX_CONSOLE_STACK_SIZE;
                }
            }
        }
        else if (next)
        {
            m_ConsoleSelectedIndex = 0;
        }

        SetTextW(m_ConsoleStack[m_ConsoleSelectedIndex]);
        SetPos((int)m_ConsoleStack[m_ConsoleSelectedIndex].length());
        m_PositionChanged = true;
    }
}

void CGameConsole::ClearStack()
{
    m_ConsoleStack[0] = {};
    m_ConsoleStackCount = 0;
    m_ConsoleSelectedIndex = 0;
    m_PositionChanged = false;
}

wstr_t CGameConsole::GetLastConsoleText()
{
    return m_ConsoleStack[m_ConsoleStackCount - 1];
}

bool CGameConsole::ConsoleTypeIsEmpty(GAME_CONSOLE_TEXT_TYPE type)
{
    bool result = (g_GameConsole.Length() == 0);
    if (result)
        return result;

    const wstr_t prefix = wstr_from(s_ConsolePrefix[type]);
    switch (type)
    {
        case GCTT_YELL:
        case GCTT_WHISPER:
        case GCTT_EMOTE:
        case GCTT_C:
        case GCTT_BROADCAST:
        case GCTT_PARTY:
        {
            result = g_GameConsole.GetTextW().compare(prefix);
            break;
        }
        default:
            break;
    }

    return result;
}

void CGameConsole::DeleteConsoleTypePrefix(GAME_CONSOLE_TEXT_TYPE type)
{
    const wstr_t prefix = wstr_from(s_ConsolePrefix[type]);
    switch (type)
    {
        case GCTT_YELL:
        case GCTT_WHISPER:
        case GCTT_EMOTE:
        case GCTT_C:
        case GCTT_BROADCAST:
        case GCTT_PARTY:
        {
            auto str = g_GameConsole.GetTextW();
            if (str.find(prefix) == 0)
            {
                str.erase(str.begin(), str.begin() + prefix.length());
                g_GameConsole.SetTextW(str);
            }

            break;
        }
        default:
            break;
    }
}

void CGameConsole::SetConsoleTypePrefix(GAME_CONSOLE_TEXT_TYPE type)
{
    const wstr_t prefix = wstr_from(s_ConsolePrefix[type]);
    switch (type)
    {
        case GCTT_YELL:
        case GCTT_WHISPER:
        case GCTT_EMOTE:
        case GCTT_C:
        case GCTT_BROADCAST:
        case GCTT_PARTY:
        {
            auto str = prefix + g_GameConsole.GetTextW();
            g_GameConsole.SetTextW(str);
            break;
        }
        default:
            break;
    }
}

const char *CGameConsole::GetConsoleTypePrefix(GAME_CONSOLE_TEXT_TYPE type)
{
    return s_ConsolePrefix[type];
}
