// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/datawriter.cpp), byte-exact including the
// original Move() auto-resize/push_back behavior (Do dress .coa format
// compatibility in later rounds).

#include "datawriter.h"

namespace xuoassist {

CDataWriter::CDataWriter()
    : m_AutoResize(true)
    , m_Ptr(nullptr)
{
}

CDataWriter::CDataWriter(size_t size, bool autoResize)
    : m_AutoResize(autoResize)
{
    m_Data.resize(size, 0);
    m_Ptr = &m_Data[0];
}

CDataWriter::~CDataWriter()
{
    m_Data.clear();
    m_Ptr = nullptr;
}

void CDataWriter::Resize(size_t newSize, bool resetPtr)
{
    m_Data.resize(newSize, 0);

    if (resetPtr)
    {
        m_Ptr = &m_Data[0];
    }
}

void CDataWriter::Move(size_t offset)
{
    if (m_AutoResize)
    {
        for (int i = 0; i < static_cast<int>(offset); i++)
        {
            m_Data.push_back(0);
        }
    }
    else if (m_Ptr != nullptr)
    {
        m_Ptr += offset;
    }
}

void CDataWriter::WriteDataBE(const uint8_t *data, size_t size, size_t offset)
{
    if (m_AutoResize)
    {
        for (int i = static_cast<int>(size) - 1; i >= 0; i--)
        {
            m_Data.push_back(data[i]);
        }
    }
    else if (m_Ptr != nullptr)
    {
        uint8_t *ptr = m_Ptr + offset + size - 1;
        for (int i = static_cast<int>(size) - 1; i >= 0; i--)
        {
            *(ptr - i) = data[i];
        }
        m_Ptr += size;
    }
}

void CDataWriter::WriteDataLE(const uint8_t *data, size_t size, size_t offset)
{
    if (m_AutoResize)
    {
        for (int i = 0; i < static_cast<int>(size); i++)
        {
            m_Data.push_back(data[i]);
        }
    }
    else if (m_Ptr != nullptr)
    {
        uint8_t *ptr = m_Ptr + offset;
        for (int i = 0; i < static_cast<int>(size); i++)
        {
            ptr[i] = data[i];
        }
        m_Ptr += size;
    }
}

void CDataWriter::WriteString(const string &val, size_t length, bool nullptrTerminated, size_t offset)
{
    if (length == 0)
    {
        length = val.length();
    }

    if (length <= val.length())
    {
        WriteDataLE((const uint8_t *)val.c_str(), length, offset);
    }
    else
    {
        WriteDataLE((const uint8_t *)val.c_str(), val.length(), offset);
        Move(length - val.length());
    }

    if (nullptrTerminated)
    {
        uint8_t nullptrTerminator = 0;
        WriteDataBE((const uint8_t *)&nullptrTerminator, sizeof(uint8_t), offset);
    }
}

void CDataWriter::WriteWString(
    const std::wstring &val, size_t length, bool bigEndian, bool nullptrTerminated, size_t offset)
{
    if (length == 0)
    {
        length = val.length();
    }

    m_Ptr += offset;
    size_t size = val.length();

    if (length <= size)
    {
        if (bigEndian)
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(length); i++)
            {
                WriteUInt16BE(val[i]);
            }
        }
        else
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(length); i++)
            {
                WriteUInt16LE(val[i]);
            }
        }
    }
    else
    {
        if (bigEndian)
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(size); i++)
            {
                WriteUInt16BE(val[i]);
            }
        }
        else
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(size); i++)
            {
                WriteUInt16LE(val[i]);
            }
        }

        Move((length - size) * 2);
    }

    if (nullptrTerminated)
    {
        ushort nullptrTerminator = 0;
        WriteDataBE((uint8_t *)&nullptrTerminator, sizeof(uint16_t), offset);
    }

    m_Ptr -= offset;
}

} // namespace xuoassist
