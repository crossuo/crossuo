// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "EntryText.h"
const int MAX_CONSOLE_STACK_SIZE = 200;

enum GAME_CONSOLE_TEXT_TYPE
{
    GCTT_NORMAL = 0,
    GCTT_YELL,
    GCTT_WHISPER,
    GCTT_EMOTE,
    GCTT_C,
    GCTT_BROADCAST,
    GCTT_PARTY,
    GCTT_GUILD,
    GCTT_ALLIANCE,
    GCTT_PARTY_ACCEPT,
    GCTT_PARTY_DECLINE,
    GCTT_PARTY_ADD,
    GCTT_PARTY_LEAVE
};

void SendConsoleText(wstr_t text, uint16_t defaultColor = 0);

class CGameConsole : public CEntryText
{
private:
    GAME_CONSOLE_TEXT_TYPE m_Type = GCTT_NORMAL;
    wstr_t m_ConsoleStack[MAX_CONSOLE_STACK_SIZE];
    int m_ConsoleStackCount = 0;
    int m_ConsoleSelectedIndex = 0;
    bool m_PositionChanged = false;

public:
    CGameConsole();
    virtual ~CGameConsole();

    void DrawW(
        uint8_t font,
        uint16_t color,
        int x,
        int y,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0) override;

    bool InChat() const;
    void Send();
    void SaveConsoleMessage();
    void ChangeConsoleMessage(bool next);
    void ClearStack();
    wstr_t GetLastConsoleText();
    virtual bool Insert(char16_t ch, CGump *gump = nullptr) override;

    static bool ConsoleTypeIsEmpty(GAME_CONSOLE_TEXT_TYPE type);
    static void DeleteConsoleTypePrefix(GAME_CONSOLE_TEXT_TYPE type);
    static void SetConsoleTypePrefix(GAME_CONSOLE_TEXT_TYPE type);
    static const char *GetConsoleTypePrefix(GAME_CONSOLE_TEXT_TYPE type);
};

extern CGameConsole g_GameConsole;
