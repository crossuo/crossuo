// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "UOHuffman.h"
#include "Wisp.h"

class CSocket : public Wisp::CConnection
{
public:
    bool GameSocket = false;
    bool UseProxy = false;
    astr_t ProxyAddress = "";
    uint16_t ProxyPort = 0;
    bool ProxySocks5 = false;
    astr_t ProxyAccount = "";
    astr_t ProxyPassword = "";

private:
    CDecompressingCopier m_Decompressor;

public:
    CSocket(bool gameSocket);
    ~CSocket();

    virtual bool Connect(const astr_t &address, uint16_t port);
    virtual std::vector<uint8_t> Decompression(std::vector<uint8_t> data);
};
