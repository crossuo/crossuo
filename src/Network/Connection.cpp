// MIT License
// Copyright (C) August 2016 Hotride

#include "Connection.h"
#include "../Sockets.h"
#include "../Crypt/CryptEntry.h"

CSocket::CSocket(bool gameSocket)
    : GameSocket(gameSocket)
{
}

CSocket::~CSocket()
{
}

bool CSocket::Connect(const string &address, uint16_t port)
{
    DEBUG_TRACE_FUNCTION;
    Info(Network, "Connecting...%s:%i", address.c_str(), port);

    if (UseProxy)
    {
        if (Connected)
        {
            return false;
        }
        Info(Network, "Connecting using proxy %s:%d", ProxyAddress.c_str(), ProxyPort);
        if (!CConnection::Connect(ProxyAddress, ProxyPort))
        {
            Info(Network, "Can't connect to proxy");
            m_Socket = nullptr;
            Connected = false;
            Info(Network, "Connecting...%s:%i", address.c_str(), port);
            return Wisp::CConnection::Connect(address, port);
        }

        uint16_t serverPort = htons(port);
        uint32_t serverIP = inet_addr(address.c_str());

        if (serverIP == 0xFFFFFFFF)
        {
            struct hostent *uohe = gethostbyname(address.c_str());

            if (uohe != nullptr)
            {
                sockaddr_in caddr;
                memcpy(&caddr.sin_addr, uohe->h_addr, uohe->h_length);
#if defined(XUO_WINDOWS)
                serverIP = caddr.sin_addr.S_un.S_addr;
#else
                serverIP = caddr.sin_addr.s_addr;
#endif
            }
        }

        if (serverIP == 0xFFFFFFFF)
        {
            Info(Network, "Unknowm server address");
            tcp_close(m_Socket);
            m_Socket = nullptr;
            Connected = false;
            Info(Network, "Connecting...%s:%i", address.c_str(), port);
            return Wisp::CConnection::Connect(address, port);
        }

        if (ProxySocks5)
        {
            Info(Network, "Proxy Server Version 5 Selected");
            unsigned char str[255] = { 0 };
            str[0] = 5; //Proxy Version
            str[1] = 2; //Number of authentication method
            str[2] = 0; //No auth required
            str[3] = 2; //Username/Password auth
            tcp_send(m_Socket, str, 4);
            int num = tcp_recv(m_Socket, str, 255);
            if ((str[0] != 5) || (num != 2))
            {
                Info(Network, "Proxy Server Version Missmatch");
                tcp_close(m_Socket);
                m_Socket = nullptr;
                Connected = false;
                Info(Network, "Connecting...%s:%i", address.c_str(), port);
                return Wisp::CConnection::Connect(address, port);
            }

            if ((str[1] == 0) || (str[1] == 2))
            {
                if (str[1] == 2)
                {
                    Info(Network, "Proxy wants Username/Password");
                    int totalSize = 3 + (int)ProxyAccount.length() + (int)ProxyPassword.length();
                    vector<char> buffer(totalSize, 0);
                    sprintf(&buffer[0], "  %s %s", ProxyAccount.c_str(), ProxyPassword.c_str());
                    buffer[0] = 1;
                    buffer[1] = (char)ProxyAccount.length();
                    buffer[2 + (int)ProxyAccount.length()] = (char)ProxyPassword.length();
                    tcp_send(m_Socket, (unsigned char *)&buffer[0], totalSize);
                    tcp_recv(m_Socket, str, 255);
                    if (str[1] != 0)
                    {
                        Info(Network, "Wrong Username/Password");
                        tcp_close(m_Socket);
                        m_Socket = nullptr;
                        Connected = false;
                        Info(Network, "Connecting...%s:%i", address.c_str(), port);
                        return Wisp::CConnection::Connect(address, port);
                    }
                }
                memset(str, 0, 10);
                str[0] = 5;
                str[1] = 1;
                str[2] = 0;
                str[3] = 1;
                memcpy(&str[4], &serverIP, 4);
                memcpy(&str[8], &serverPort, 2);
                tcp_send(m_Socket, str, 10);
                num = tcp_recv(m_Socket, str, 255);
                if (str[1] != 0)
                {
                    switch (str[1])
                    {
                        case 1:
                            Info(Network, "general SOCKS server failure");
                            break;
                        case 2:
                            Info(Network, "connection not allowed by ruleset");
                            break;
                        case 3:
                            Info(Network, "Network unreachable");
                            break;
                        case 4:
                            Info(Network, "Host unreachable");
                            break;
                        case 5:
                            Info(Network, "Connection refused");
                            break;
                        case 6:
                            Info(Network, "TTL expired");
                            break;
                        case 7:
                            Info(Network, "Command not supported");
                            break;
                        case 8:
                            Info(Network, "Address type not supported");
                            break;
                        case 9:
                            Info(Network, "to X'FF' unassigned");
                            break;
                        default:
                            Info(Network, "Unknown Error <%d> recieved", str[1]);
                    }

                    tcp_close(m_Socket);
                    m_Socket = nullptr;
                    Connected = false;
                    Info(Network, "Connecting...%s:%i", address.c_str(), port);
                    return Wisp::CConnection::Connect(address, port);
                }
                Info(Network, "Connected to server via proxy");
            }
            else
            {
                Info(Network, "No acceptable methods");
                tcp_close(m_Socket);
                m_Socket = nullptr;
                Connected = false;
                Info(Network, "Connecting...%s:%i", address.c_str(), port);
                return Wisp::CConnection::Connect(address, port);
            }
        }
        else
        {
            Info(Network, "Proxy Server Version 4 Selected");
            unsigned char str[9] = { 0 };
            str[0] = 4;
            str[1] = 1;
            memcpy(&str[2], &serverPort, 2);
            memcpy(&str[4], &serverIP, 4);
            tcp_send(m_Socket, str, 9);
            int recvSize = tcp_recv(m_Socket, str, 8);
            if ((recvSize != 8) || (str[0] != 0) || (str[1] != 90))
            {
                if (str[0] == 5)
                {
                    Info(Network, "Proxy Server Version is 5");
                    Info(Network, "Trying  SOCKS5");
                    tcp_close(m_Socket);
                    m_Socket = nullptr;
                    Connected = false;
                    ProxySocks5 = true;
                    return Connect(address, port);
                }
                switch (str[1])
                {
                    case 1:
                    case 91:
                        Info(Network, "Proxy request rejected or failed");
                        break;
                    case 2:
                    case 92:
                        Info(
                            Network,
                            "Proxy rejected becasue SOCKS server cannot connect to identd on the client");
                        break;
                    case 3:
                    case 93:
                        Info(
                            Network,
                            "Proxy rejected becasue SOCKS server cannot connect to identd on the client");
                        break;
                    default:
                        Info(Network, "Unknown Error <%d> recieved", str[1]);
                        break;
                }
                tcp_close(m_Socket);
                m_Socket = nullptr;
                Connected = false;
                Info(Network, "Connecting...%s:%i", address.c_str(), port);
                return Wisp::CConnection::Connect(address, port);
            }
            Info(Network, "Connected to server via proxy");
        }
    }
    else
    {
        return Wisp::CConnection::Connect(address, port);
    }

    return true;
}

vector<uint8_t> CSocket::Decompression(vector<uint8_t> data)
{
    DEBUG_TRACE_FUNCTION;
    if (GameSocket)
    {
        auto inSize = (intptr_t)data.size();
        Crypt::Decrypt(&data[0], &data[0], (int)inSize);

        vector<uint8_t> decBuf(inSize * 4 + 2);
        int outSize = 65536;
        m_Decompressor((char *)&decBuf[0], (char *)&data[0], outSize, inSize);
        if (inSize != data.size())
        {
            TRACE(Network, "decompression buffer too small");
            Disconnect();
        }
        else
        {
            decBuf.resize(outSize);
        }
        return decBuf;
    }
    return data;
}
