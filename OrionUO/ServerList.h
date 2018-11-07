// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CServer
{
public:
    uint16_t Index = 0;
    string Name = "";
    uint8_t FullPercent = 0;
    uint8_t Timezone = 0;
    uint32_t IP = 0;
    bool Selected = false;
    int Ping = -1;
    int PacketsLoss = -1;

    CServer();
    CServer(
        uint16_t index,
        const string &name,
        uint8_t fullPercent,
        uint8_t timezone,
        int ip,
        bool selected);
    ~CServer();
};

class CServerList
{
public:
    string LastServerName = "";
    uint32_t LastServerIndex = 0;

private:
    vector<CServer> m_Servers;

public:
    CServerList();
    ~CServerList();

    CServer *GetServer(int index);
    CServer *GetSelectedServer();
    int ServersCount() { return (int)m_Servers.size(); }
    CServer *Select(int index);
    void ParsePacket(Wisp::CDataReader &reader);
};

extern CServerList g_ServerList;
