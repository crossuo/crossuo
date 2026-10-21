// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <vector>
#include <stdint.h>

// growable byte queue that splits the socket stream into complete packets;
// the packet-size callback decodes each packet's length prefix
class CPacketMessage
{
    bool BigEndian = false;
    std::vector<uint8_t> m_Data;

public:
    CPacketMessage(bool bigEndian = true);

    void Append(uint8_t *data, int dataSize);
    void Append(const std::vector<uint8_t> &data);

    void Clear() { m_Data.clear(); }

    std::vector<uint8_t>
    Read(int (*packetSize)(const std::vector<uint8_t> &packet, int &offsetToSize), int &dataOffset);
};
