// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/datareader.h): pointer-arithmetic buffer,
// byte-exact (bounds behavior preserved). Deliberately NOT unified with
// xuocore's CDataReader this round (plan §4.1). string/wstring -> std::.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CDataReader
{
    SETGET(uint8_t *, Start, nullptr)
    SETGET(size_t, Size, 0)
    SETGET(uint8_t *, End, nullptr)
    SETGET(uint8_t *, Ptr, nullptr)
    SETGET(uint64_t, Hash, 0)

public:
    CDataReader();
    CDataReader(uint8_t *start, size_t size);

    virtual ~CDataReader();

    void SetData(uint8_t *start, size_t size, size_t offset = 0);
    void ResetPtr() { m_Ptr = m_Start; }

    bool IsEOF() { return m_Ptr >= m_End; }

    void Move(size_t offset) { m_Ptr += offset; }

    void ReadDataBE(uint8_t *data, size_t size, size_t offset = 0);
    void ReadDataLE(uint8_t *data, size_t size, size_t offset = 0);

    uint8_t ReadUInt8(size_t offset = 0)
    {
        uint8_t val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(uint8_t), offset);
        return val;
    }

    ushort ReadUInt16BE(size_t offset = 0)
    {
        ushort val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(uint16_t), offset);
        return val;
    }

    ushort ReadUInt16LE(size_t offset = 0)
    {
        ushort val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(uint16_t), offset);
        return val;
    }

    uint ReadUInt32BE(size_t offset = 0)
    {
        uint val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(uint32_t), offset);
        return val;
    }

    uint ReadUInt32LE(size_t offset = 0)
    {
        uint val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(uint32_t), offset);
        return val;
    }

    char ReadInt8(size_t offset = 0)
    {
        char val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(int8_t), offset);
        return val;
    }

    short ReadInt16BE(size_t offset = 0)
    {
        short val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(int16_t), offset);
        return val;
    }
    short ReadInt16LE(size_t offset = 0)
    {
        short val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(int16_t), offset);
        return val;
    }

    int ReadInt32BE(size_t offset = 0)
    {
        int val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(int32_t), offset);
        return val;
    }
    int ReadInt32LE(size_t offset = 0)
    {
        int val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(int32_t), offset);
        return val;
    }

    int64_t ReadInt64BE(size_t offset = 0)
    {
        int64_t val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(int64_t), offset);
        return val;
    }
    int64_t ReadInt64LE(size_t offset = 0)
    {
        int64_t val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(int64_t), offset);
        return val;
    }

    float ReadFloatBE(size_t offset = 0)
    {
        float val = 0.0f;
        ReadDataBE((uint8_t *)&val, sizeof(float), offset);
        return val;
    }
    float ReadFloatLE(size_t offset = 0)
    {
        float val = 0.0f;
        ReadDataLE((uint8_t *)&val, sizeof(float), offset);
        return val;
    }

    double ReadDoubleBE(size_t offset = 0)
    {
        double val = 0.0;
        ReadDataBE((uint8_t *)&val, sizeof(double), offset);
        return val;
    }
    double ReadDoubleLE(size_t offset = 0)
    {
        double val = 0.0;
        ReadDataLE((uint8_t *)&val, sizeof(double), offset);
        return val;
    }

    string ReadString(size_t size = 0, size_t offset = 0);
    std::wstring ReadWString(size_t size = 0, bool bigEndian = true, size_t offset = 0);
};

} // namespace xuoassist

