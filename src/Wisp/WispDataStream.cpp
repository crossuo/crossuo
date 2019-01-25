// MIT License

#if 0
#define DATASTREAM_DEBUG DEBUG_TRACE_FUNCTION
#else
#define DATASTREAM_DEBUG
#endif

namespace Wisp
{
CDataWriter::CDataWriter()
{
}

CDataWriter::CDataWriter(size_t size, bool autoResize)
    : AutoResize(autoResize)
{
    DATASTREAM_DEBUG;
    m_Data.resize(size, 0);
    Ptr = &m_Data[0];
}

CDataWriter::~CDataWriter()
{
    DATASTREAM_DEBUG;
    m_Data.clear();
    Ptr = nullptr;
}

void CDataWriter::Resize(size_t newSize, bool resetPtr)
{
    DATASTREAM_DEBUG;
    m_Data.resize(newSize, 0);

    if (resetPtr)
    {
        Ptr = &m_Data[0];
    }
}

void CDataWriter::Move(const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    if (AutoResize)
    {
        for (int i = 0; i < (int)offset; i++)
        {
            m_Data.push_back(0);
        }
    }
    else if (Ptr != nullptr)
    {
        Ptr += offset;
    }
}

void CDataWriter::WriteDataBE(const uint8_t *data, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    if (AutoResize)
    {
        for (int i = (int)size - 1; i >= 0; i--)
        {
            m_Data.push_back(data[i]);
        }
    }
    else if (Ptr != nullptr)
    {
        uint8_t *ptr = Ptr + offset + size - 1;

        for (int i = (int)size - 1; i >= 0; i--)
        {
            *(ptr - i) = data[i];
        }

        Ptr += size;
    }
}

void CDataWriter::WriteDataLE(const uint8_t *data, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    if (AutoResize)
    {
        for (int i = 0; i < (int)size; i++)
        {
            m_Data.push_back(data[i]);
        }
    }
    else if (Ptr != nullptr)
    {
        uint8_t *ptr = Ptr + offset;

        for (int i = 0; i < (int)size; i++)
        {
            ptr[i] = data[i];
        }

        Ptr += size;
    }
}

void CDataWriter::WriteString(
    const string &val, size_t length, bool nullTerminated, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    if (length == 0u)
    {
        length = val.length();
    }

    if (length <= val.length())
    {
        WriteDataLE((uint8_t *)val.c_str(), length, offset);
    }
    else
    {
        WriteDataLE((uint8_t *)val.c_str(), val.length(), offset);
        Move(length - val.length());
    }

    if (nullTerminated)
    {
        uint8_t nullTerminator = 0;
        WriteDataBE((uint8_t *)&nullTerminator, sizeof(uint8_t), offset);
    }
}

void CDataWriter::WriteWString(
    const wstring &val, size_t length, bool bigEndian, bool nullTerminated, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    if (length == 0u)
    {
        length = val.length();
    }

    Ptr += offset;
    size_t size = val.length();

    if (length <= size)
    {
        if (bigEndian)
        {
            for (int i = 0; i < (int)length; i++)
            {
                WriteInt16BE(val[i]);
            }
        }
        else
        {
            for (int i = 0; i < (int)length; i++)
            {
                WriteInt16LE(val[i]);
            }
        }
    }
    else
    {
        if (bigEndian)
        {
            for (int i = 0; i < (int)size; i++)
            {
                WriteInt16BE(val[i]);
            }
        }
        else
        {
            for (int i = 0; i < (int)size; i++)
            {
                WriteInt16LE(val[i]);
            }
        }

        Move((length - size) * 2);
    }

    if (nullTerminated)
    {
        uint16_t nullTerminator = 0;
        WriteDataBE((uint8_t *)&nullTerminator, sizeof(uint16_t), offset);
    }

    Ptr -= offset;
}

CDataReader::CDataReader()
{
}

CDataReader::CDataReader(uint8_t *start, size_t size)
    : Start(start)
    , Size(size)
    , End(Start + size)
{
    DATASTREAM_DEBUG;
    Ptr = Start;
}

CDataReader::~CDataReader()
{
    DATASTREAM_DEBUG;
    Start = nullptr;
    Size = 0;
    End = nullptr;
    Ptr = nullptr;
}

void CDataReader::SetData(uint8_t *start, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    Start = start;
    Size = size;
    End = Start + size;
    Ptr = Start + offset;
}

void CDataReader::ReadDataBE(uint8_t *data, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    if (Ptr != nullptr)
    {
        uint8_t *ptr = Ptr + offset + size - 1;
        if (ptr >= Start && ptr <= End)
        {
            for (int i = 0; i < (int)size; i++)
            {
                data[i] = *(ptr - i);
            }

            Ptr += size;
        }
    }
}

void CDataReader::ReadDataLE(uint8_t *data, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    if (Ptr != nullptr)
    {
        uint8_t *ptr = Ptr + offset;
        if (ptr >= Start && ptr + size <= End)
        {
            memcpy(data, ptr, size);
            Ptr += size;
        }
    }
}

string CDataReader::ReadString(size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG;

    uint8_t *ptr = Ptr + offset;
    if (size == 0u)
    {
        if (ptr >= Start && ptr <= End)
        {
            uint8_t *buf = ptr;
            while (buf <= End && (*buf != 0u))
            {
                buf++;
            }
            size = (buf - ptr) + 1;
        }
    }

    auto *buf = (char *)alloca(size + 1);
    if (ptr >= Start && ptr + size <= End)
    {
        ReadDataLE((uint8_t *)buf, size, offset);
    }
    buf[size] = '\0';

    return { buf };
}

wstring CDataReader::ReadWString(size_t size, bool bigEndian, const intptr_t &offset)
{
    DATASTREAM_DEBUG;

    uint8_t *ptr = Ptr + offset;
    if (size == 0u)
    {
        if (ptr >= Start && ptr <= End)
        {
            uint8_t *buf = ptr;
            while (buf <= End)
            {
                uint16_t val = (bigEndian ? ((buf[0] << 8) | buf[1]) : *(uint16_t *)buf);
                buf += 2;
                if (val == 0u)
                {
                    break;
                }
            }

            size = ((buf - ptr) / 2);
        }
    }

    auto *buf = (wchar_t *)alloca((size + 1) * sizeof(wchar_t));
    if (ptr >= Start && ptr + size <= End)
    {
        if (bigEndian)
        {
            for (int i = 0; i < (int)size; i++)
            {
                buf[i] = (wchar_t)ReadInt16BE(offset);
            }
        }
        else
        {
            for (int i = 0; i < (int)size; i++)
            {
                buf[i] = (wchar_t)ReadInt16LE(offset);
            }
        }
    }
    buf[size] = 0;

    return { buf };
}

wstring CDataReader::ReadWStringLE(size_t size, const intptr_t &offset)
{
    return ReadWString(size, false, offset);
}

wstring CDataReader::ReadWStringBE(size_t size, const intptr_t &offset)
{
    return ReadWString(size, true, offset);
}

}; // namespace Wisp
