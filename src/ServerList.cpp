// MIT License
// Copyright (C) August 2016 Hotride

#include "ServerList.h"
#include "CrossUO.h"
#include "ScreenStages/MainScreen.h"
#include "ScreenStages/ServerScreen.h"
#include "Utility/PingThread.h"

CServerList g_ServerList;

CServer::CServer()
{
}

CServer::CServer(
    uint16_t index,
    const string &name,
    uint8_t fullPercent,
    uint8_t timezone,
    int ip,
    bool selected)
    : Index(index)
    , Name(name)
    , FullPercent(fullPercent)
    , Timezone(timezone)
    , IP(ip)
    , Selected(selected)
{
}

CServer::~CServer()
{
}

CServerList::CServerList()
{
}

CServerList::~CServerList()
{
    m_Servers.clear();
}

void CServerList::ParsePacket(Wisp::CDataReader &reader)
{
    m_Servers.clear();
    g_ServerList.LastServerIndex = 0;

    reader.Move(1);

    uint16_t numServers = reader.ReadUInt16BE();

    if (numServers == 0)
    {
        Warning(Network, "empty server list");
    }

    for (uint16_t i = 0; i < numServers; i++)
    {
        uint16_t id = reader.ReadUInt16BE();
        string name = reader.ReadString(32);
        uint8_t fullPercent = reader.ReadUInt8();
        uint8_t timezone = reader.ReadUInt8();
        uint32_t ip = reader.ReadUInt32LE(); //little-endian!!!

        const bool selected = (name == g_ServerList.LastServerName);
        if (selected)
        {
            g_ServerList.LastServerIndex = (int)i;
        }
        m_Servers.push_back(CServer(id, name, fullPercent, timezone, ip, selected));

#if USE_PING
        if (!g_DisablePing)
        {
            char ipString[30] = { 0 };
            sprintf_s(
                ipString,
                "%i.%i.%i.%i",
                (ip >> 24) & 0xFF,
                (ip >> 16) & 0xFF,
                (ip >> 8) & 0xFF,
                ip & 0xFF);
            CPingThread *pingThread = new CPingThread(i, ipString, 100);
            pingThread->Run();
        }
#endif // USE_PING
    }

    if (g_ServerList.LastServerIndex < numServers && g_MainScreen.m_AutoLogin->Checked)
    {
        g_Game.ServerSelection(g_ServerList.LastServerIndex);
    }
    else
    {
        g_Game.InitScreen(GS_SERVER);
    }

    g_ServerScreen.UpdateContent();
}

CServer *CServerList::GetServer(int index)
{
    DEBUG_TRACE_FUNCTION;
    if (index < (int)m_Servers.size())
    {
        return &m_Servers[index];
    }

    return nullptr;
}

CServer *CServerList::GetSelectedServer()
{
    DEBUG_TRACE_FUNCTION;

    for (CServer &server : m_Servers)
    {
        if (server.Selected)
        {
            return &server;
        }
    }

    return nullptr;
}

CServer *CServerList::Select(int index)
{
    DEBUG_TRACE_FUNCTION;
    CServer *server = nullptr;

    for (int i = int(m_Servers.size()) - 1; i >= 0; i--)
    {
        if (index == i)
        {
            server = &m_Servers[i];
            m_Servers[i].Selected = true;
        }
        else
        {
            m_Servers[i].Selected = false;
        }
    }

    return server;
}
