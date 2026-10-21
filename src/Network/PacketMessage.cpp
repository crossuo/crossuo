// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "PacketMessage.h"
#include "../Misc.h" // PACKET_VARIABLE_SIZE

CPacketMessage::CPacketMessage(bool bigEndian)
    : BigEndian(bigEndian)
{
}

void CPacketMessage::Append(uint8_t *data, int dataSize)
{
    m_Data.insert(m_Data.end(), data, data + dataSize);
}

void CPacketMessage::Append(const std::vector<uint8_t> &data)
{
    m_Data.insert(m_Data.end(), data.begin(), data.end());
}

std::vector<uint8_t>
CPacketMessage::Read(int (*packetSize)(const std::vector<uint8_t> &packet, int &offsetToSize), int &dataOffset)
{
    std::vector<uint8_t> result;

    if (m_Data.empty())
    {
        return result;
    }

    int offsetToSize = 0;
    int wantSize = packetSize(m_Data, offsetToSize);

    if (wantSize == PACKET_VARIABLE_SIZE)
    {
        if (m_Data.size() < 3)
        {
            return result;
        }

        uint8_t *data = &m_Data[1];

        if (BigEndian)
        {
            wantSize = (data[0] << 8) | data[1];
        }
        else
        {
            wantSize = (data[1] << 8) | data[0];
        }

        dataOffset = offsetToSize + 2;
    }
    else
    {
        dataOffset = offsetToSize;
    }

    if ((int)m_Data.size() < wantSize)
    {
        return result;
    }

    result.insert(result.begin(), m_Data.begin(), m_Data.begin() + wantSize);

    m_Data.erase(m_Data.begin(), m_Data.begin() + wantSize);

    return result;
}
