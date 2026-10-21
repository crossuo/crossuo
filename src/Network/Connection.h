// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "UOHuffman.h"
#include "Wisp.h"

class CSocket : public Wisp::CConnection
{
public:
    bool GameSocket = false;

private:
    CDecompressingCopier m_Decompressor;

public:
    CSocket(bool gameSocket);
    ~CSocket();

    virtual bool Connect(const astr_t &address, uint16_t port);
    virtual std::vector<uint8_t> Decompression(std::vector<uint8_t> data);
};
