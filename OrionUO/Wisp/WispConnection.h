#pragma once

#include "../Sockets.h"
namespace Wisp
{
class CConnection
{
public:
    tcp_socket m_Socket = nullptr;

    int DataReady = 0;
    int Port = 0;

    bool Connected = 0;

    CConnection();
    virtual ~CConnection();
    void Init();

    class CPacketMessage *m_MessageParser;

    virtual bool Connect(const string &address, uint16_t port);
    void Disconnect();

    bool ReadyRead();

    virtual vector<uint8_t> Decompression(vector<uint8_t> data) { return data; }

    bool Read(int maxSize = 0x1000);

    int Send(uint8_t *data, int size);
    int Send(const vector<uint8_t> &data);
};
}; // namespace Wisp
