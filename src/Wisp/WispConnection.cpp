

#include "WispConnection.h"
#include "WispPacketMessage.h"
#include "../Logging.h"

namespace Wisp
{
CConnection::CConnection()
{
}

void CConnection::Init()
{
    m_Socket = tcp_open();
    m_MessageParser = new CPacketMessage();
}

CConnection::~CConnection()
{
    Disconnect();

    if (m_MessageParser != nullptr)
    {
        delete m_MessageParser;
        m_MessageParser = nullptr;
    }
}

bool CConnection::Connect(const std::string &address, uint16_t port)
{
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

void CConnection::Disconnect()
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

bool CConnection::ReadyRead()
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

bool CConnection::Read(int maxSize)
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
            //TRACE(Network, "CConnection::Read size=%i", size);
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

int CConnection::Send(uint8_t *data, int size)
{
    if (!Connected || m_Socket == nullptr)
    {
        return 0;
    }

    const int sent = tcp_send(m_Socket, data, size);
    //Info(Network,  "CConnection::Send=>%i", sent);
    return sent;
}

int CConnection::Send(const std::vector<uint8_t> &data)
{
    if (data.empty())
    {
        return 0;
    }

    const int sent = Send((uint8_t *)&data[0], (int)data.size());
    //TRACE(Network, "CConnection::Send=>%i", sent);
    return sent;
}
}; // namespace Wisp
