﻿
#ifndef WISP_CON_H
#define WISP_CON_H

#include <vector>
#include <stdint.h>
#include <common/str.h>

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

    virtual bool Connect(const astr_t &address, uint16_t port);
    void Disconnect();

    bool ReadyRead();

    virtual std::vector<uint8_t> Decompression(std::vector<uint8_t> data) { return data; }

    bool Read(int maxSize = 0x1000);

    int Send(uint8_t *data, int size);
    int Send(const std::vector<uint8_t> &data);
};
}; // namespace Wisp

#endif // WISP_CON_H
