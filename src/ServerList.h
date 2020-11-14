// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <vector>
#include <stdint.h>
#include <common/str.h>

struct CDataReader;

class CServer
{
public:
    uint16_t Index = 0;
    astr_t Name = "";
    uint8_t FullPercent = 0;
    uint8_t Timezone = 0;
    uint32_t IP = 0;
    bool Selected = false;
    int Ping = -1;
    int PacketsLoss = -1;

    CServer();
    CServer(
        uint16_t index,
        const astr_t &name,
        uint8_t fullPercent,
        uint8_t timezoneType,
        int ip,
        bool selected);
    ~CServer();
};

class CServerList
{
public:
    astr_t LastServerName = "";
    uint32_t LastServerIndex = 0;

private:
    std::vector<CServer> m_Servers;

public:
    CServerList();
    ~CServerList();

    CServer *GetServer(int index);
    CServer *GetSelectedServer();
    int ServersCount() { return (int)m_Servers.size(); }
    CServer *Select(int index);
    void ParsePacket(CDataReader &reader);
};

extern CServerList g_ServerList;
