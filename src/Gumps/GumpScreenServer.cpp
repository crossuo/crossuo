// MIT License
// Copyright (C) August 2016 Hotride

#include <common/str.h>
#include "GumpScreenServer.h"
#include "../Config.h"
#include "../ToolTip.h"
#include "../ServerList.h"
#include "../SelectedObject.h"
#include "../Managers/ClilocManager.h"
#include "../Managers/ConfigManager.h"
#include "../ScreenStages/ServerScreen.h"

enum
{
    ID_SS_NONE,

    ID_SS_QUIT,
    ID_SS_ARROW_PREV,
    ID_SS_ARROW_NEXT,
    ID_SS_TIME_ZONE,
    ID_SS_FULL,
    ID_SS_CONNECTION,
    ID_SS_EARTH,
    ID_SS_HTML_GUMP,
    ID_SS_SERVER_LIST,

    ID_SS_COUNT,
};

CGumpScreenServer::CGumpScreenServer()
    : CGump(GT_NONE, 0, 0, 0)
{
    NoMove = true;
    NoClose = true;
}

CGumpScreenServer::~CGumpScreenServer()
{
}

void CGumpScreenServer::UpdateContent()
{
    Clear();

    Add(new CGUIGumppicTiled(0x0E14, 0, 0, 640, 480));
    Add(new CGUIGumppic(0x157C, 0, 0));
    Add(new CGUIGumppic(0x15A0, 0, 4));
    Add(new CGUIButton(ID_SS_QUIT, 0x1589, 0x158A, 0x158B, 555, 4));
    Add(new CGUIButton(ID_SS_ARROW_PREV, 0x15A1, 0x15A2, 0x15A3, 586, 445));
    Add(new CGUIButton(ID_SS_ARROW_NEXT, 0x15A4, 0x15A5, 0x15A6, 610, 445));

    CCliloc *cliloc = g_ClilocManager.Cliloc(g_Language);

    uint16_t textColor = 0x0481;
    if (g_Config.ClientVersion >= CV_500A)
    {
        textColor = 0xFFFF;
    }

    CGUIText *text = new CGUIText(textColor, 155, 70);

    if (g_Config.ClientVersion >= CV_500A)
    {
        text->CreateTextureW(0, cliloc->GetW(1044579, false, "Select which shard to play on:"));
    }
    else
    {
        text->CreateTextureA(9, "Select which shard to play on:");
    }

    Add(text);

    text = new CGUIText(textColor, 400, 70);

    if (g_Config.ClientVersion >= CV_500A)
    {
        text->CreateTextureW(0, cliloc->GetW(1044577, false, "Latency:"));
    }
    else
    {
        text->CreateTextureA(9, "Latency:");
    }

    Add(text);

    text = new CGUIText(textColor, 470, 70);

    if (g_Config.ClientVersion >= CV_500A)
    {
        text->CreateTextureW(0, cliloc->GetW(1044578, false, "Packet Loss:"));
    }
    else
    {
        text->CreateTextureA(9, "Packet Loss:");
    }

    Add(text);

    CGUIHTMLGump *htmlGump = (CGUIHTMLGump *)Add(
        new CGUIHTMLGump(ID_SS_HTML_GUMP, 0x0DAC, 150, 90, 393, 271, true, true));

    htmlGump->m_Scissor->SetY(htmlGump->m_Scissor->GetY() + 16);
    htmlGump->m_Scissor->Height -= 32;

    text = new CGUIText(textColor, 153, 368);

    if (g_Config.ClientVersion >= CV_500A)
    {
        text->CreateTextureW(0, cliloc->GetW(1044580, false, "Sort by:"));
    }
    else
    {
        text->CreateTextureA(9, "Sort by:");
    }

    Add(text);

    Add(new CGUIButton(ID_SS_TIME_ZONE, 0x093B, 0x093D, 0x093C, 230, 366));
    Add(new CGUIButton(ID_SS_FULL, 0x093E, 0x0940, 0x093F, 338, 366));
    Add(new CGUIButton(ID_SS_CONNECTION, 0x0941, 0x0943, 0x0942, 446, 366));

    Add(new CGUIGumppic(0x0589, 150, 390));

    Add(new CGUIButton(ID_SS_EARTH, 0x15E8, 0x15E9, 0x15EA, 160, 400));

    int offsetY = 0;

    for (int i = 0; i < g_ServerList.ServersCount(); i++)
    {
        CServer *server = g_ServerList.GetServer((uint32_t)i);

        htmlGump->Add(new CGUIHitBox(ID_SS_SERVER_LIST + (int)i, 74, 10 + ((int)i * 25), 280, 25));

        CGUITextEntry *entry = new CGUITextEntry(
            ID_SS_SERVER_LIST + (int)i, 0x034F, 0x0021, 0x0021, 74, 10 + offsetY, 0, false, 5);
        entry->m_Entry.SetTextA(server->Name);
        entry->ReadOnly = true;
        entry->CheckOnSerial = true;
        htmlGump->Add(entry);

        entry = new CGUITextEntry(
            ID_SS_SERVER_LIST + (int)i, 0x034F, 0x0021, 0x0021, 250, 10 + offsetY, 0, false, 5);
#if USE_PING
        if (server->Ping == -1)
        {
            entry->m_Entry.SetTextA("-");
        }
        else
        {
            entry->m_Entry.SetTextA(str_from(server->Ping) + "ms");
        }
#else
        entry->m_Entry.SetTextA("-");
#endif // USE_PING

        entry->ReadOnly = true;
        entry->CheckOnSerial = true;
        htmlGump->Add(entry);

        entry = new CGUITextEntry(
            ID_SS_SERVER_LIST + (int)i, 0x034F, 0x0021, 0x0021, 320, 10 + offsetY, 0, false, 5);
        if (server->PacketsLoss == -1)
        {
            entry->m_Entry.SetTextA("-");
        }
        else
        {
            entry->m_Entry.SetTextA(str_from(server->PacketsLoss) + "%");
        }
        entry->ReadOnly = true;
        entry->CheckOnSerial = true;
        htmlGump->Add(entry);

        offsetY += 25;
    }

    htmlGump->CalculateDataSize();

    if (g_ServerList.ServersCount() > 0)
    {
        text = (CGUIText *)Add(new CGUIText(0x0481, 243, 420));
        if (g_ServerList.LastServerIndex < (uint32_t)g_ServerList.ServersCount())
        {
            text->CreateTextureA(9, g_ServerList.GetServer(g_ServerList.LastServerIndex)->Name);
        }
        else
        {
            text->CreateTextureA(9, g_ServerList.GetServer(0)->Name);
        }
    }
}

void CGumpScreenServer::InitToolTip()
{
    if (!g_ConfigManager.UseToolTips || g_SelectedObject.Object == nullptr)
    {
        return;
    }

    uint32_t id = g_SelectedObject.Serial;

    switch (id)
    {
        case ID_SS_QUIT:
        {
            g_ToolTip.Set(L"Quit Ultima Online", 80);
            break;
        }
        case ID_SS_ARROW_PREV:
        {
            g_ToolTip.Set(L"Back to main menu");
            break;
        }
        case ID_SS_ARROW_NEXT:
        {
            g_ToolTip.Set(L"Play UO on selected shard");
            break;
        }
        case ID_SS_EARTH:
        {
            g_ToolTip.Set(L"Select last visited shard", 100);
            break;
        }
        default:
            break;
    }

    if (id >= ID_SS_SERVER_LIST)
    {
        astr_t cstr(
            "Connect to '" + g_ServerList.GetServer(id - ID_SS_SERVER_LIST)->Name + "' server");

        g_ToolTip.Set(wstr_from(cstr), 100);
    }
}

void CGumpScreenServer::GUMP_BUTTON_EVENT_C
{
    if (serial == ID_SS_QUIT)
    { //x button
        g_ServerScreen.CreateSmoothAction(CServerScreen::ID_SMOOTH_SS_QUIT);
    }
    else if (serial == ID_SS_ARROW_PREV)
    { //< button
        g_ServerScreen.CreateSmoothAction(CServerScreen::ID_SMOOTH_SS_GO_SCREEN_MAIN);
    }
    else if (serial == ID_SS_ARROW_NEXT || serial == ID_SS_EARTH) //> button
    {
        g_ServerScreen.SelectionServerTempValue = g_ServerList.LastServerIndex;
        g_ServerScreen.CreateSmoothAction(CServerScreen::ID_SMOOTH_SS_SELECT_SERVER);
    }
}

void CGumpScreenServer::GUMP_TEXT_ENTRY_EVENT_C
{
    if (serial >= ID_SS_SERVER_LIST) //Server selection
    {
        g_ServerScreen.SelectionServerTempValue = serial - ID_SS_SERVER_LIST;
        g_ServerScreen.CreateSmoothAction(CServerScreen::ID_SMOOTH_SS_SELECT_SERVER);
    }
}
