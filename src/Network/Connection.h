// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <vector>
#include <common/str.h>
#include "../Sockets.h"
#include "UOHuffman.h"
#include "PacketMessage.h"

class CSocket
{
public:
    tcp_socket m_Socket = nullptr;

    int DataReady = 0;
    int Port = 0;

    bool Connected = 0;

    bool GameSocket = false;

    class CPacketMessage *m_MessageParser;

private:
    CDecompressingCopier m_Decompressor;

public:
    CSocket(bool gameSocket);
    ~CSocket();

    void Init();
    bool Connect(const astr_t &address, uint16_t port);
    void Disconnect();

    bool ReadyRead();

    std::vector<uint8_t> Decompression(std::vector<uint8_t> data);

    bool Read(int maxSize = 0x1000);

    int Send(uint8_t *data, int size) const;
    int Send(const std::vector<uint8_t> &data);
};
