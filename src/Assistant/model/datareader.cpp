// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/datareader.cpp), byte-exact: the original
// bounds checks (ptr >= m_Start && ptr <= m_End / ptr + size <= m_End) and the
// no-op on out-of-bounds reads are preserved as-is.

#include "datareader.h"

namespace xuoassist {

CDataReader::CDataReader()
{
}

CDataReader::CDataReader(uint8_t *start, size_t size)
    : m_Start(start)
    , m_Size(size)
    , m_End(m_Start + size)
{
    m_Ptr = m_Start;
}

CDataReader::~CDataReader()
{
    m_Start = nullptr;
    m_Size = 0;
    m_End = nullptr;
    m_Ptr = nullptr;
}

void CDataReader::SetData(uint8_t *start, size_t size, size_t offset)
{
    m_Start = start;
    m_Size = size;
    m_End = m_Start + size;
    m_Ptr = m_Start + offset;
}

void CDataReader::ReadDataBE(uint8_t *data, size_t size, size_t offset)
{
    if (m_Ptr != nullptr)
    {
        uint8_t *ptr = m_Ptr + offset + size - 1;
        if (ptr >= m_Start && ptr <= m_End)
        {
            for (int i = 0; i < static_cast<int>(size); i++)
            {
                data[i] = *(ptr - i);
            }
            m_Ptr += size;
        }
    }
}

void CDataReader::ReadDataLE(uint8_t *data, size_t size, size_t offset)
{
    if (m_Ptr != nullptr)
    {
        uint8_t *ptr = m_Ptr + offset;
        if (ptr >= m_Start && ptr + size <= m_End)
        {
            for (int i = 0; i < static_cast<int>(size); i++)
            {
                data[i] = ptr[i];
            }
            m_Ptr += size;
        }
    }
}

string CDataReader::ReadString(size_t size, size_t offset)
{
    uint8_t *ptr = m_Ptr + offset;
    if (size == 0)
    {
        if (ptr >= m_Start && ptr <= m_End)
        {
            uint8_t *buf = ptr;
            while (buf <= m_End && (*buf != 0u))
            {
                buf++;
            }
            size = size_t(buf - ptr) + 1;
        }
    }

    string result;
    if (ptr >= m_Start && ptr + size <= m_End)
    {
        result.resize(size, 0);
        ReadDataLE((uint8_t *)&result[0], size, offset);
    }

    return result;
}

std::wstring CDataReader::ReadWString(size_t size, bool bigEndian, size_t offset)
{
    uint8_t *ptr = m_Ptr + offset;
    if (size == 0)
    {
        if (ptr >= m_Start && ptr <= m_End)
        {
            uint8_t *buf = ptr;
            while (buf <= m_End)
            {
                auto val = uint16_t(bigEndian ? ((buf[0] << 8) | buf[1]) : *(uint16_t *)buf);
                buf += 2;
                if (val == 0u)
                {
                    break;
                }
            }
            size = size_t((buf - ptr) / 2);
        }
    }

    std::wstring result;
    if (ptr >= m_Start && ptr + size <= m_End)
    {
        result.resize(size, 0);
        if (bigEndian)
        {
            for (size_t i = 0; i < size; i++)
            {
                result[i] = ReadUInt16BE(offset);
            }
        }
        else
        {
            for (size_t i = 0; i < size; i++)
            {
                result[i] = ReadUInt16LE(offset);
            }
        }
    }
    return result;
}

} // namespace xuoassist
