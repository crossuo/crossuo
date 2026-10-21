// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "../Network/Connection.h"

class CConnectionManager
{
private:
    CSocket m_LoginSocket{ CSocket(false) };
    CSocket m_GameSocket{ CSocket(true) };
    // true - m_LoginSocket, false - m_GameSocket
    bool m_IsLoginSocket = true;
    // Set when the login socket is closed ON PURPOSE as part of the relay
    // (0x8C) handoff to the game server. Recv() checks it so an intentional
    // teardown is not misread as a connection loss (which would bounce the
    // client back to the main screen and re-run the whole login - the web
    // double-dial).
    bool m_LoginHandoff = false;
    uint8_t m_Seed[4];

    void SendIP(CSocket &socket, uint8_t *ip);

public:
    CConnectionManager();
    ~CConnectionManager();

    void Init();
    void Init(uint8_t *gameSeed);
    bool Connected() { return (m_LoginSocket.Connected || m_GameSocket.Connected); }
    bool Connect(const astr_t &address, int port, uint8_t *gameSeed);
    void Disconnect();
    void Recv();
    int Send(uint8_t *buf, int size);
    int Send(const std::vector<uint8_t> &data);
    const uint8_t *GetClientIP() const { return &m_Seed[0]; }
};

extern CConnectionManager g_ConnectionManager;
