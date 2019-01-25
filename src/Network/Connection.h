// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "UOHuffman.h"

class CSocket : public Wisp::CConnection
{
public:
    bool GameSocket = false;
    bool UseProxy = false;
    string ProxyAddress = "";
    uint16_t ProxyPort = 0;
    bool ProxySocks5 = false;
    string ProxyAccount = "";
    string ProxyPassword = "";

private:
    CDecompressingCopier m_Decompressor;

public:
    CSocket(bool gameSocket);
    ~CSocket();

    virtual bool Connect(const string &address, uint16_t port);
    virtual vector<uint8_t> Decompression(vector<uint8_t> data);
};
