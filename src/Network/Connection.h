// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "UOHuffman.h"
#include "Wisp.h"

class CSocket : public Wisp::CConnection
{
public:
    bool GameSocket = false;
    bool UseProxy = false;
    std::string ProxyAddress = "";
    uint16_t ProxyPort = 0;
    bool ProxySocks5 = false;
    std::string ProxyAccount = "";
    std::string ProxyPassword = "";

private:
    CDecompressingCopier m_Decompressor;

public:
    CSocket(bool gameSocket);
    ~CSocket();

    virtual bool Connect(const std::string &address, uint16_t port);
    virtual std::vector<uint8_t> Decompression(std::vector<uint8_t> data);
};
