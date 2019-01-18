// MIT License

#ifndef WISP_DW_H
#define WISP_DW_H

namespace Wisp
{
class CDataWriter
{
public:
    bool AutoResize = true;
    uint8_t *Ptr = 0;

protected:
    vector<uint8_t> m_Data;

public:
    CDataWriter();
    CDataWriter(size_t size, bool autoResize = true);

    virtual ~CDataWriter();

    vector<uint8_t> Data() const { return m_Data; }
    uint8_t *DataPtr() { return &m_Data[0]; }
    size_t Size() { return m_Data.size(); }

    void Resize(size_t newSize, bool resetPtr = false);
    void ResetPtr() { Ptr = &m_Data[0]; }

    void Move(const intptr_t &offset);

    void WriteDataBE(const uint8_t *data, size_t size, const intptr_t &offset = 0);
    void WriteDataLE(const uint8_t *data, size_t size, const intptr_t &offset = 0);

    void WriteUInt8(uint8_t val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(uint8_t), offset);
    }

    void WriteUInt16BE(uint16_t val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(uint16_t), offset);
    }
    void WriteUInt16LE(uint16_t val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(uint16_t), offset);
    }

    void WriteUInt32BE(int val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(uint32_t), offset);
    }
    void WriteUInt32LE(int val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(uint32_t), offset);
    }

    void WriteUInt64BE(uint64_t val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(uint64_t), offset);
    }
    void WriteUInt64LE(uint64_t val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(uint64_t), offset);
    }

    void WriteInt8(char val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(char), offset);
    }

    void WriteInt16BE(short val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(short), offset);
    }
    void WriteInt16LE(short val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(short), offset);
    }

    void WriteInt32BE(int val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(int), offset);
    }
    void WriteInt32LE(int val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(int), offset);
    }

    void WriteInt64BE(int64_t val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(int64_t), offset);
    }
    void WriteInt64LE(int64_t val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(int64_t), offset);
    }

    void WriteFloatBE(float val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(float), offset);
    }
    void WriteFloatLE(float val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(float), offset);
    }

    void WriteDoubleBE(double val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(double), offset);
    }
    void WriteDoubleLE(double val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(double), offset);
    }

    void WriteString(
        const string &val,
        size_t length = 0,
        bool nullTerminated = true,
        const intptr_t &offset = 0);
    void WriteWString(
        const wstring &val,
        size_t length = 0,
        bool bigEndian = true,
        bool nullTerminated = true,
        const intptr_t &offset = 0);
};

class CDataReader
{
public:
    uint8_t *Start = 0;
    size_t Size = 0;
    uint8_t *End = 0;
    uint8_t *Ptr = 0;

    CDataReader();
    CDataReader(uint8_t *start, size_t size);

    virtual ~CDataReader();

    void SetData(uint8_t *start, size_t size, const intptr_t &offset = 0);
    void ResetPtr() { Ptr = Start; }

    bool IsEOF() { return Ptr >= End; }

    void Move(const intptr_t &offset) { Ptr += offset; }

    void ReadDataBE(uint8_t *data, size_t size, const intptr_t &offset = 0);
    void ReadDataLE(uint8_t *data, size_t size, const intptr_t &offset = 0);

    uint8_t ReadUInt8(const intptr_t &offset = 0)
    {
        uint8_t val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(uint8_t), offset);
        return val;
    }

    uint16_t ReadUInt16BE(const intptr_t &offset = 0)
    {
        uint16_t val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(uint16_t), offset);
        return val;
    }
    uint16_t ReadUInt16LE(const intptr_t &offset = 0)
    {
        uint16_t val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(uint16_t), offset);
        return val;
    }

    uint32_t ReadUInt32BE(const intptr_t &offset = 0)
    {
        uint32_t val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(uint32_t), offset);
        return val;
    }
    uint32_t ReadUInt32LE(const intptr_t &offset = 0)
    {
        uint32_t val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(uint32_t), offset);
        return val;
    }

    char ReadInt8(const intptr_t &offset = 0)
    {
        char val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(char), offset);
        return val;
    }

    short ReadInt16BE(const intptr_t &offset = 0)
    {
        short val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(short), offset);
        return val;
    }
    short ReadInt16LE(const intptr_t &offset = 0)
    {
        short val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(short), offset);
        return val;
    }

    int ReadInt32BE(const intptr_t &offset = 0)
    {
        int val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(int), offset);
        return val;
    }
    int ReadInt32LE(const intptr_t &offset = 0)
    {
        int val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(int), offset);
        return val;
    }

    int64_t ReadInt64BE(const intptr_t &offset = 0)
    {
        int64_t val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(int64_t), offset);
        return val;
    }
    int64_t ReadInt64LE(const intptr_t &offset = 0)
    {
        int64_t val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(int64_t), offset);
        return val;
    }

    uint64_t ReadUInt64BE(const intptr_t &offset = 0)
    {
        uint64_t val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(uint64_t), offset);
        return val;
    }
    uint64_t ReadUInt64LE(const intptr_t &offset = 0)
    {
        uint64_t val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(uint64_t), offset);
        return val;
    }

    float ReadFloatBE(const intptr_t &offset = 0)
    {
        float val = 0.0f;
        ReadDataBE((uint8_t *)&val, sizeof(float), offset);
        return val;
    }
    float ReadFloatLE(const intptr_t &offset = 0)
    {
        float val = 0.0f;
        ReadDataLE((uint8_t *)&val, sizeof(float), offset);
        return val;
    }

    double ReadDoubleBE(const intptr_t &offset = 0)
    {
        double val = 0.0;
        ReadDataBE((uint8_t *)&val, sizeof(double), offset);
        return val;
    }
    double ReadDoubleLE(const intptr_t &offset = 0)
    {
        double val = 0.0;
        ReadDataLE((uint8_t *)&val, sizeof(double), offset);
        return val;
    }

    string ReadString(size_t size = 0, const intptr_t &offset = 0);
    wstring ReadWStringBE(size_t size = 0, const intptr_t &offset = 0);
    wstring ReadWStringLE(size_t size = 0, const intptr_t &offset = 0);

private:
    wstring ReadWString(size_t size = 0, bool bigEndian = true, const intptr_t &offset = 0);
};

}; // namespace Wisp

#endif // WISP_DW_H
