#if 0
#define DATASTREAM_DEBUG(x) DATASTREAM_DEBUG(x)
#else
#define DATASTREAM_DEBUG(x)
#endif

namespace Wisp
{
CDataWritter::CDataWritter()
{
}

CDataWritter::CDataWritter(size_t size, bool autoResize)
    : AutoResize(autoResize)
{
    DATASTREAM_DEBUG("c4_f1");
    m_Data.resize(size, 0);
    Ptr = &m_Data[0];
}

CDataWritter::~CDataWritter()
{
    DATASTREAM_DEBUG("c4_f2");
    m_Data.clear();
    Ptr = nullptr;
}

void CDataWritter::Resize(size_t newSize, bool resetPtr)
{
    DATASTREAM_DEBUG("c4_f3");
    m_Data.resize(newSize, 0);

    if (resetPtr)
        Ptr = &m_Data[0];
}

void CDataWritter::Move(const intptr_t &offset)
{
    DATASTREAM_DEBUG("c4_f4");
    if (AutoResize)
    {
        for (int i = offset; i < 0; i++)
            m_Data.push_back(0);
    }
    else if (Ptr != nullptr)
        Ptr += offset;
}

void CDataWritter::WriteDataBE(const puchar data, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG("c4_f5");
    if (AutoResize)
    {
        DFOR (i, size - 1, 0)
            m_Data.push_back(data[i]);
    }
    else if (Ptr != nullptr)
    {
        puchar ptr = Ptr + offset + size - 1;

        DFOR (i, size - 1, 0)
            *(ptr - i) = data[i];

        Ptr += size;
    }
}

void CDataWritter::WriteDataLE(const puchar data, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG("c4_f6");
    if (AutoResize)
    {
        for (int i = 0; i < size; i++)
            m_Data.push_back(data[i]);
    }
    else if (Ptr != nullptr)
    {
        puchar ptr = Ptr + offset;

        for (int i = 0; i < size; i++)
            ptr[i] = data[i];

        Ptr += size;
    }
}

void CDataWritter::WriteString(
    const string &val, size_t length, bool nullTerminated, const intptr_t &offset)
{
    DATASTREAM_DEBUG("c4_f7");
    if (!length)
        length = val.length();

    if (length <= val.length())
        WriteDataLE((puchar)val.c_str(), length, offset);
    else
    {
        WriteDataLE((puchar)val.c_str(), val.length(), offset);
        Move(length - val.length());
    }

    if (nullTerminated)
    {
        uchar nullTerminator = 0;
        WriteDataBE((puchar)&nullTerminator, sizeof(uchar), offset);
    }
}

void CDataWritter::WriteWString(
    const wstring &val, size_t length, bool bigEndian, bool nullTerminated, const intptr_t &offset)
{
    DATASTREAM_DEBUG("c4_f8");
    if (!length)
        length = val.length();

    Ptr += offset;
    size_t size = val.length();

    if (length <= size)
    {
        if (bigEndian)
        {
            for (int i = 0; i < length; i++)
                WriteInt16BE(val[i]);
        }
        else
        {
            for (int i = 0; i < length; i++)
                WriteInt16LE(val[i]);
        }
    }
    else
    {
        if (bigEndian)
        {
            for (int i = 0; i < size; i++)
                WriteInt16BE(val[i]);
        }
        else
        {
            for (int i = 0; i < size; i++)
                WriteInt16LE(val[i]);
        }

        Move((length - size) * 2);
    }

    if (nullTerminated)
    {
        ushort nullTerminator = 0;
        WriteDataBE((puchar)&nullTerminator, sizeof(ushort), offset);
    }

    Ptr -= offset;
}

//------------------------------------CDataReader-----------------------------------

CDataReader::CDataReader()
{
}

CDataReader::CDataReader(puchar start, size_t size)
    : Start(start)
    , Size(size)
    , End(Start + size)
{
    DATASTREAM_DEBUG("c5_f1");
    Ptr = Start;
}

CDataReader::~CDataReader()
{
    DATASTREAM_DEBUG("c5_f2");
    Start = nullptr;
    Size = 0;
    End = nullptr;
    Ptr = nullptr;
}

void CDataReader::SetData(puchar start, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG("c5_f3");
    Start = start;
    Size = size;
    End = Start + size;
    Ptr = Start + offset;
}

void CDataReader::ReadDataBE(puchar data, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG("c5_f4");
    if (Ptr != nullptr)
    {
        puchar ptr = Ptr + offset + size - 1;

        if (ptr >= Start && ptr <= End)
        {
            for (int i = 0; i < size; i++)
                data[i] = *(ptr - i);

            Ptr += size;
        }
    }
}

void CDataReader::ReadDataLE(puchar data, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG("c5_f5");
    if (Ptr != nullptr)
    {
        puchar ptr = Ptr + offset;

        if (ptr >= Start && ptr + size <= End)
        {
            for (int i = 0; i < size; i++)
                data[i] = ptr[i];

            Ptr += size;
        }
    }
}

string CDataReader::ReadString(size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG("c5_f6");
    puchar ptr = Ptr + offset;

    if (!size)
    {
        if (ptr >= Start && ptr <= End)
        {
            puchar buf = ptr;

            while (buf <= End && *buf)
                buf++;

            size = (buf - ptr) + 1;
        }
    }

    string result = "";

    if (ptr >= Start && ptr + size <= End)
    {
        result.resize(size, 0);
        ReadDataLE((puchar)&result[0], size, offset);
    }

    return result.c_str(); // This must be here to trim null bytes
}

wstring CDataReader::ReadWString(size_t size, bool bigEndian, const intptr_t &offset)
{
    DATASTREAM_DEBUG("c5_f7");
    puchar ptr = Ptr + offset;

    if (!size)
    {
        if (ptr >= Start && ptr <= End)
        {
            puchar buf = ptr;

            while (buf <= End)
            {
                ushort val = (bigEndian ? ((buf[0] << 8) | buf[1]) : *(pushort)buf);

                buf += 2;

                if (!val)
                    break;
            }

            size = ((buf - ptr) / 2);
        }
    }

    wstring result = L"";

    if (ptr >= Start && ptr + size <= End)
    {
        result.resize(size, 0);

        if (bigEndian)
        {
            for (int i = 0; i < size; i++)
                result[i] = ReadInt16BE(offset);
        }
        else
        {
            for (int i = 0; i < size; i++)
                result[i] = ReadInt16LE(offset);
        }
    }

    return result.c_str(); // This must be here to trim null bytes
}

}; // namespace Wisp
