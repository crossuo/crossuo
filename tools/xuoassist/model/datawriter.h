// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/datawriter.h): the pointless Q_OBJECT base
// is dropped (plan §4.1). Deliberately NOT unified with xuocore's CDataWriter
// this round (plan §4.1). vector<uint8_t> stays std::vector<uint8_t>.

#ifndef XUOASSIST_DATAWRITTER_H
#define XUOASSIST_DATAWRITTER_H

#include "core/global.h"

namespace xuoassist {

class CDataWriter
{
    SETGET(bool, AutoResize, false)
    SETGET(uint8_t *, Ptr, nullptr)

protected:
    vector<uint8_t> m_Data;

public:
    CDataWriter();
    CDataWriter(size_t size, bool autoResize = true);

    virtual ~CDataWriter();

    vector<uint8_t> Data() const { return m_Data; }
    size_t Size() const { return m_Data.size(); }

    void Resize(size_t newSize, bool resetPtr = false);
    void ResetPtr() { m_Ptr = &m_Data[0]; }

    void Move(size_t offset);

    void WriteDataBE(const uint8_t *data, size_t size, size_t offset = 0);
    void WriteDataLE(const uint8_t *data, size_t size, size_t offset = 0);

    void WriteUInt8(uint8_t val, size_t offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(uint8_t), offset);
    }

    void WriteUInt16BE(uint16_t val, size_t offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(uint16_t), offset);
    }

    void WriteUInt16LE(uint16_t val, size_t offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(uint16_t), offset);
    }

    void WriteUInt32BE(uint32_t val, size_t offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(uint32_t), offset);
    }

    void WriteUInt32LE(uint32_t val, size_t offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(uint32_t), offset);
    }

    void WriteUInt64BE(uint64_t val, size_t offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(uint64_t), offset);
    }

    void WriteUInt64LE(uint64_t val, size_t offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(uint64_t), offset);
    }

    void WriteInt8(int8_t val, size_t offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(int8_t), offset);
    }

    void WriteInt16BE(int16_t val, size_t offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(int16_t), offset);
    }

    void WriteInt16LE(int16_t val, size_t offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(int16_t), offset);
    }

    void WriteInt32BE(int32_t val, size_t offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(int32_t), offset);
    }

    void WriteInt32LE(int32_t val, size_t offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(int32_t), offset);
    }

    void WriteInt64BE(int64_t val, size_t offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(int64_t), offset);
    }

    void WriteInt64LE(int64_t val, size_t offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(int64_t), offset);
    }

    void WriteFloatBE(float val, size_t offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(float), offset);
    }

    void WriteFloatLE(float val, size_t offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(float), offset);
    }

    void WriteDoubleBE(double val, size_t offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(double), offset);
    }

    void WriteDoubleLE(double val, size_t offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(double), offset);
    }

    void WriteString(
        const string &val, size_t length = 0, bool nullptrTerminated = true, size_t offset = 0);
    void WriteWString(
        const std::wstring &val,
        size_t length = 0,
        bool bigEndian = true,
        bool nullptrTerminated = true,
        size_t offset = 0);
};

} // namespace xuoassist

#endif // XUOASSIST_DATAWRITTER_H
