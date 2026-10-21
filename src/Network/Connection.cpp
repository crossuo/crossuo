// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "Connection.h"
#include <common/logging/logging.h>
#include <cstdint>
#include "PacketMessage.h"
#include "../Sockets.h"
#include "../Crypt/CryptEntry.h"

CSocket::CSocket(bool gameSocket)
    : GameSocket(gameSocket)
{
}

CSocket::~CSocket()
{
    Disconnect();

    if (m_MessageParser != nullptr)
    {
        delete m_MessageParser;
        m_MessageParser = nullptr;
    }
}

void CSocket::Init()
{
    m_Socket = tcp_open();
    m_MessageParser = new CPacketMessage();
}

bool CSocket::Connect(const astr_t &address, uint16_t port)
{
    Info(Network, "Connecting...%s:%i", address.c_str(), port);

    if (Connected)
    {
        return false;
    }

    if (m_Socket == nullptr)
    {
        m_Socket = tcp_open();
    }

    if (m_Socket == nullptr)
    {
        return false;
    }

    if (!tcp_connect(m_Socket, address.c_str(), port))
    {
        return false;
    }

    Port = port;
    Connected = true;
    m_MessageParser->Clear();

    return true;
}

void CSocket::Disconnect()
{
    if (Connected && m_Socket != nullptr)
    {
        tcp_close(m_Socket);
        m_Socket = nullptr;
        Connected = false;
        DataReady = 0;
        Port = 0;
        m_MessageParser->Clear();
    }
}

bool CSocket::ReadyRead()
{
    if (!Connected || m_Socket == nullptr)
    {
        return false;
    }

    DataReady = tcp_select(m_Socket);
    if (DataReady == -1)
    {
        Error(Network, "CConnection::ReadyRead SOCKET_ERROR");
        Disconnect();
    }

    return (DataReady != 0);
}

bool CSocket::Read(int maxSize)
{
    if (DataReady == -1)
    {
        Error(Network, "CConnection::Read, m_DataReady=%i", DataReady);
        Disconnect();
    }
    else if (Connected && m_Socket != nullptr)
    {
        std::vector<uint8_t> data(maxSize);
        const int size = tcp_recv(m_Socket, &data[0], maxSize);

        if (size > 0)
        {
            data.resize(size);
            data = Decompression(data);
            m_MessageParser->Append(data);
            return true;
        }

        Error(Network, "CConnection::Read, bad size=%i", size);
    }
    else
    {
        Error(Network, "CConnection::Read, unknown state, m_Connected=%i", Connected);
    }
    return false;
}

int CSocket::Send(uint8_t *data, int size) const
{
    if (!Connected || m_Socket == nullptr)
    {
        return 0;
    }

    const int sent = tcp_send(m_Socket, data, size);
    return sent;
}

int CSocket::Send(const std::vector<uint8_t> &data)
{
    if (data.empty())
    {
        return 0;
    }

    return Send((uint8_t *)&data[0], (int)data.size());
}

std::vector<uint8_t> CSocket::Decompression(std::vector<uint8_t> data)
{
    if (GameSocket)
    {
        auto inSize = (intptr_t)data.size();
        Crypt::Decrypt(&data[0], &data[0], (int)inSize);

        std::vector<uint8_t> decBuf(inSize * 4 + 2);
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
