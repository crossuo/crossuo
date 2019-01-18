// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "EntryText.h"

class CGameConsole : public CEntryText
{
private:
    GAME_CONSOLE_TEXT_TYPE m_Type = GCTT_NORMAL;
    wstring m_ConsoleStack[MAX_CONSOLE_STACK_SIZE];
    int m_ConsoleStackCount = 0;
    int m_ConsoleSelectedIndex = 0;
    bool m_PositionChanged = false;

public:
    CGameConsole();
    virtual ~CGameConsole();

    static void Send(wstring text, uint16_t defaultColor = 0);
    static wstring
    IsSystemCommand(const wchar_t *text, size_t &len, int &member, GAME_CONSOLE_TEXT_TYPE &type);

    void DrawW(
        uint8_t font,
        uint16_t color,
        int x,
        int y,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    bool InChat() const;
    void Send();
    void SaveConsoleMessage();
    void ChangeConsoleMessage(bool next);
    void ClearStack();
};

static const wstring g_ConsolePrefix[] = {
    L"",    //Normal
    L"! ",  //Yell
    L"; ",  //Whisper
    L": ",  //Emote
    L".",   //Command
    L"? ",  //Broadcast
    L"/ ",  //Party
    L"\\ ", //Guild
    L"| "   //Alliance
};

extern CGameConsole g_GameConsole;
