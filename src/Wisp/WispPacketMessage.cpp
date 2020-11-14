// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "WispPacketReader.h"
#include "WispPacketMessage.h"
#include "../Misc.h"

namespace Wisp
{
CPacketMessage::CPacketMessage(bool bigEndian)
    : BigEndian(bigEndian)
{
}

CPacketMessage::CPacketMessage(uint8_t *data, int dataSize, bool bigEndian)
    : BigEndian(bigEndian)
{
    m_Data.resize(dataSize);
    memcpy(&m_Data[0], &data[0], dataSize);
}

CPacketMessage::CPacketMessage(const std::vector<uint8_t> &data, bool bigEndian)
    : BigEndian(bigEndian)
    , m_Data(data)
{
}

CPacketMessage::~CPacketMessage()
{
    m_Data.clear();
}

void CPacketMessage::Append(uint8_t *data, int dataSize)
{
    std::vector<uint8_t> buf(dataSize);
    memcpy(&buf[0], &data[0], dataSize);

    m_Data.insert(m_Data.end(), buf.begin(), buf.end());
}

void CPacketMessage::Append(const std::vector<uint8_t> &data)
{
    m_Data.insert(m_Data.end(), data.begin(), data.end());
}

std::vector<uint8_t> CPacketMessage::Read(CPacketReader *reader, int &dataOffset)
{
    std::vector<uint8_t> result;

    if (m_Data.empty())
    {
        return result;
    }

    int offsetToSize = 0;
    int wantSize = reader->GetPacketSize(m_Data, offsetToSize);

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

}; // namespace Wisp
