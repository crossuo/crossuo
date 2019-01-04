// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CConnectionManager
{
protected:
    bool m_UseProxy = false;

public:
    bool GetUseProxy() { return m_UseProxy; };
    void SetUseProxy(bool val);

protected:
    string m_ProxyAddress = "";

public:
    string GetProxyAddress() { return m_ProxyAddress; };
    void SetProxyAddress(const string &val);

protected:
    int m_ProxyPort = 0;

public:
    int GetProxyPort() { return m_ProxyPort; };
    void SetProxyPort(int val);

protected:
    bool m_ProxySocks5 = false;

public:
    bool GetProxySocks5() { return m_ProxySocks5; };
    void SetProxySocks5(bool val);

protected:
    string m_ProxyAccount = "";

public:
    string GetProxyAccount() { return m_ProxyAccount; };
    void SetProxyAccount(const string &val);

protected:
    string m_ProxyPassword = "";

public:
    string GetProxyPassword() { return m_ProxyPassword; };
    void SetProxyPassword(const string &val);

private:
    CSocket m_LoginSocket{ CSocket(false) };
    CSocket m_GameSocket{ CSocket(true) };
    // true - m_LoginSocket, false - m_GameSocket
    bool m_IsLoginSocket = true;
    uint8_t m_Seed[4];

    void SendIP(CSocket &socket, uint8_t *ip);

public:
    CConnectionManager();
    ~CConnectionManager();

    void Init();
    void Init(uint8_t *gameSeed);
    bool Connected() { return (m_LoginSocket.Connected || m_GameSocket.Connected); }
    bool Connect(const string &address, int port, uint8_t *gameSeed);
    void Disconnect();
    void Recv();
    int Send(uint8_t *buf, int size);
    int Send(const vector<uint8_t> &data);
    const uint8_t *GetClientIP() const { return &m_Seed[0]; }
};

extern CConnectionManager g_ConnectionManager;
