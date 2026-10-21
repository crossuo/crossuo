// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "ServerList.h"
#include "Config.h" // g_Config (ProtocolClientVersion era gates)
#include "CrossUO.h"
#include "ScreenStages/MainScreen.h"
#include "ScreenStages/ServerScreen.h"
#include <xuocore/mappedfile.h>

CServerList g_ServerList;

CServer::CServer()
{
}

CServer::CServer(
    uint16_t index,
    const astr_t &name,
    uint8_t fullPercent,
    uint8_t timezoneType,
    int ip,
    bool selected)
    : Index(index)
    , Name(name)
    , FullPercent(fullPercent)
    , Timezone(timezoneType)
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

void CServerList::ParsePacket(CDataReader &reader)
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
        auto name = reader.ReadString(32);
        uint8_t fullPercent = reader.ReadUInt8();
        uint8_t timezoneType = reader.ReadUInt8();
        // The record IP is a big-endian u32 on the wire (the 1998 client
        // ntohl()s it at 0x4603e0); modern servers send it little-endian.
        // The only consumer is the ping string below, which expects the
        // first octet in the high byte.
        uint32_t ip = (g_Config.ProtocolClientVersion < CV_200) ? reader.ReadUInt32BE() :
                                                                  reader.ReadUInt32LE();

        const bool selected = (name == g_ServerList.LastServerName);
        if (selected)
        {
            g_ServerList.LastServerIndex = (int)i;
        }
        m_Servers.push_back(CServer(id, name, fullPercent, timezoneType, ip, selected));

    }

    PostParse(numServers);
}

void CServerList::PostParse(uint16_t numServers)
{
    // the auto-login decision comes from the config (g_Config.AutoLogin,
    // set by --autologin): the checkbox mirror is populated from the CLI in
    // ProcessCommandLine, which on the web boot runs AFTER the main screen
    // was built - consulting the config directly keeps the auto-selection
    // working on both boot orders
    const bool autoLogin =
        g_Config.AutoLogin || g_MainScreen.m_AutoLogin->Checked;
    if (LastServerIndex < numServers && autoLogin)
    {
        g_Game.ServerSelection(LastServerIndex);
    }
    else
    {
        g_Game.InitScreen(GS_SERVER);
    }

    g_ServerScreen.UpdateContent();
}

// 0x81: the pre-2.0 server list (PktH_0x81_ServerListLegacy in the 1998
// client, 0x45f690). Wire, payload from the byte after the u16 length:
//   u8: character-slot count; u8: list flags (login-dialog byte, the role
//     0xA8's flags byte plays); 5x{name[30], password[30]} character slots;
//   u8: numServers; then per server {u8 index, char name[16]} (17 bytes);
//   u8: numCities; then per city {u8 index, char name[31], char area[31]}.
// Only the server records feed the shard-select screen here; the
// character/city blocks belong to the 1998 char-list flow, which crossuo
// receives via 0x86.
void CServerList::ParseLegacyPacket(CDataReader &reader)
{
    m_Servers.clear();
    LastServerIndex = 0;

    // bounded: the fixed 2 + 5*60 + 1 head must fit the frame
    if (reader.Size < reader.Pos() + 2 + 5 * 60 + 1)
    {
        return;
    }

    reader.Move(2);       // character-slot count + list flags
    reader.Move(5 * 60);  // 5x{name[30], password[30]} character slots

    const uint8_t numServers = reader.ReadUInt8();
    for (uint16_t i = 0; i < numServers; i++)
    {
        if (reader.Size - reader.Pos() < 17) // index u8 + name[16]
        {
            break;
        }

        const uint16_t id = reader.ReadUInt8();
        auto name = reader.ReadString(16);

        // No full/timezone/ip fields in the legacy record (0x45f690 reads
        // index + 16 name bytes only), so there is nothing to ping.
        const bool selected = (name == LastServerName);
        if (selected)
        {
            LastServerIndex = (int)i;
        }
        m_Servers.push_back(CServer(id, name, 0, 0, 0, selected));
    }

    PostParse(numServers);
}

CServer *CServerList::GetServer(int index)
{
    if (index < (int)m_Servers.size())
    {
        return &m_Servers[index];
    }

    return nullptr;
}

CServer *CServerList::GetSelectedServer()
{
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
