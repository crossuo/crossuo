// MIT License

#if 0
#define TEXTPARSER_TRACE_FUNCTION DEBUG_TRACE_FUNCTION
#else
#define TEXTPARSER_TRACE_FUNCTION
#endif

namespace Wisp
{
CTextFileParser::CTextFileParser(
    const os_path &path, const char *delimiters, const char *comentaries, const char *quotes)
{
    TEXTPARSER_TRACE_FUNCTION;
    if (!path.empty())
    {
        m_File.Load(path);
    }
    StartupInitalize(delimiters, comentaries, quotes);
}

CTextFileParser::~CTextFileParser()
{
    TEXTPARSER_TRACE_FUNCTION;
    m_File.Unload();
}

void CTextFileParser::StartupInitalize(
    const char *delimiters, const char *comentaries, const char *quotes)
{
    TEXTPARSER_TRACE_FUNCTION;
    memset(&m_Delimiters[0], 0, sizeof(m_Delimiters));
    m_DelimitersSize = (int)strlen(delimiters);
    if (m_DelimitersSize != 0)
    {
        memcpy(&m_Delimiters[0], &delimiters[0], m_DelimitersSize);
    }

    memset(&m_Comentaries[0], 0, sizeof(m_Comentaries));
    m_ComentariesSize = (int)strlen(comentaries);
    if (m_ComentariesSize != 0)
    {
        memcpy(&m_Comentaries[0], &comentaries[0], m_ComentariesSize);
    }

    memset(&m_Quotes[0], 0, sizeof(m_Quotes));
    m_QuotesSize = (int)strlen(quotes);
    if (m_QuotesSize != 0)
    {
        memcpy(&m_Quotes[0], &quotes[0], m_QuotesSize);
    }
    m_End = m_File.End;
}

void CTextFileParser::Restart()
{
    TEXTPARSER_TRACE_FUNCTION;
    m_File.ResetPtr();
}

bool CTextFileParser::IsEOF()
{
    TEXTPARSER_TRACE_FUNCTION;
    return (m_File.Ptr >= m_End);
}

void CTextFileParser::GetEOL()
{
    TEXTPARSER_TRACE_FUNCTION;
    m_EOL = m_File.Ptr;
    if (!IsEOF())
    {
        while (m_EOL < m_End && (*m_EOL != 0u))
        {
            if (*m_EOL == '\n')
            {
                break;
            }
            m_EOL++;
        }
    }
}

bool CTextFileParser::IsDelimiter()
{
    TEXTPARSER_TRACE_FUNCTION;
    bool result = false;
    for (int i = 0; i < m_DelimitersSize && !result; i++)
    {
        result = (*m_Ptr == m_Delimiters[i]);
    }
    return result;
}

void CTextFileParser::SkipToData()
{
    TEXTPARSER_TRACE_FUNCTION;
    while (m_Ptr < m_EOL && (*m_Ptr != 0u) && IsDelimiter())
    {
        m_Ptr++;
    }
}

bool CTextFileParser::IsComment()
{
    TEXTPARSER_TRACE_FUNCTION;
    bool result = (*m_Ptr == '\n');
    for (int i = 0; i < m_ComentariesSize && !result; i++)
    {
        result = (*m_Ptr == m_Comentaries[i]);
        if (result && i + 1 < m_ComentariesSize && m_Comentaries[i] == m_Comentaries[i + 1] &&
            m_Ptr + 1 < m_EOL)
        {
            result = (m_Ptr[0] == m_Ptr[1]);
            i++;
        }
    }

    return result;
}

bool CTextFileParser::IsQuote()
{
    bool result = (*m_Ptr == '\n');
    for (int i = 0; i < m_QuotesSize; i += 2)
    {
        if (*m_Ptr == m_Quotes[i] || *m_Ptr == m_Quotes[i + 1])
        {
            result = true;
            break;
        }
    }

    return result;
}

bool CTextFileParser::IsSecondQuote()
{
    bool result = (*m_Ptr == '\n');
    for (int i = 0; i < m_QuotesSize; i += 2)
    {
        if (*m_Ptr == m_Quotes[i + 1])
        {
            result = true;
            break;
        }
    }

    return result;
}

string CTextFileParser::ObtainData()
{
    TEXTPARSER_TRACE_FUNCTION;
    string result{};
    while (m_Ptr < m_End && (*m_Ptr != 0u) && *m_Ptr != '\n')
    {
        if (IsDelimiter())
        {
            break;
        }
        if (IsComment())
        {
            m_Ptr = m_EOL;
            break;
        }

        if (*m_Ptr != '\r' && (!m_Trim || (*m_Ptr != ' ' && *m_Ptr != '\t')))
        {
            result.push_back(*m_Ptr);
        }
        m_Ptr++;
    }

    return result;
}

string CTextFileParser::ObtainQuotedData()
{
    TEXTPARSER_TRACE_FUNCTION;
    bool exit = false;
    string result{};
    for (int i = 0; i < m_QuotesSize; i += 2)
    {
        if (*m_Ptr == m_Quotes[i])
        {
            char endQuote = m_Quotes[i + 1];
            exit = true;
            m_Ptr++;
            uint8_t *ptr = m_Ptr;
            while (ptr < m_EOL && (*ptr != 0u) && *ptr != '\n' && *ptr != endQuote)
            {
                ptr++;
            }

            size_t size = ptr - m_Ptr;
            if (size > 0)
            {
                result.resize(size + 1);
                memcpy(&result[0], &m_Ptr[0], size);
                result[size] = 0;
                for (int j = (int)size - 1; j >= 0 && result[j] == '\r'; j--)
                {
                    result[j] = 0;
                }

                m_Ptr = ptr;
                if (m_Ptr < m_EOL && *m_Ptr == endQuote)
                {
                    m_Ptr++;
                }
            }
            break;
        }
    }

    if (!exit)
    {
        result = ObtainData();
    }

    return result;
}

void CTextFileParser::SaveRawLine()
{
    TEXTPARSER_TRACE_FUNCTION;
    size_t size = m_EOL - m_Ptr;

    if (size > 0)
    {
        RawLine.resize(size, 0);
        memcpy(&RawLine[0], &m_Ptr[0], size);

        while ((RawLine.length() != 0u) && (RawLine[size - 1] == '\r' || RawLine[size - 1] == '\n'))
        {
            RawLine.resize(RawLine.length() - 1);
        }
    }
    else
    {
        RawLine = "";
    }
}

vector<string> CTextFileParser::ReadTokens(bool trim)
{
    TEXTPARSER_TRACE_FUNCTION;
    m_Trim = trim;
    vector<string> result;

    if (!IsEOF())
    {
        m_Ptr = m_File.Ptr;
        GetEOL();
        SaveRawLine();
        while (m_Ptr < m_EOL)
        {
            SkipToData();
            if (IsComment())
            {
                break;
            }

            string buf = ObtainQuotedData();
            if (buf.length() != 0u)
            {
                result.push_back(buf);
            }
            else if (IsSecondQuote())
            {
                m_Ptr++;
            }
        }
        m_File.Ptr = m_EOL + 1;
    }

    return result;
}

vector<string> CTextFileParser::GetTokens(const char *str, bool trim)
{
    TEXTPARSER_TRACE_FUNCTION;
    m_Trim = trim;
    vector<string> result;

    uint8_t *oldEnd = m_End;
    m_Ptr = (uint8_t *)str;
    m_End = (uint8_t *)str + strlen(str);
    m_EOL = m_End;

    SaveRawLine();
    while (m_Ptr < m_EOL)
    {
        SkipToData();
        if (IsComment())
        {
            break;
        }
        string buf = ObtainQuotedData();
        if (buf.length() != 0u)
        {
            result.push_back(buf);
        }
    }

    m_End = oldEnd;
    return result;
}

CTextFileWriter::CTextFileWriter(const os_path &path)
{
    m_File = fs_open(path, FS_WRITE);
}

CTextFileWriter::~CTextFileWriter()
{
    Close();
}

void CTextFileWriter::Close()
{
    if (m_File != nullptr)
    {
        fs_close(m_File);
        m_File = nullptr;
    }
}

void CTextFileWriter::WriteString(const string &key, const string &value)
{
    if (m_File != nullptr)
    {
        fputs(string(key + "=" + value + "\n").c_str(), m_File);
    }
}

void CTextFileWriter::WriteInt(const string &key, int value)
{
    if (m_File != nullptr)
    {
        fputs(string(key + "=" + std::to_string(value) + "\n").c_str(), m_File);
    }
}

void CTextFileWriter::WriteBool(const string &key, bool value)
{
    if (m_File != nullptr)
    {
        fputs(string(key + "=" + (value ? "yes" : "no") + "\n").c_str(), m_File);
    }
}

}; // namespace Wisp
