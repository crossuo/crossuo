// MIT License

#include "text_parser.h"

TextFileParser::TextFileParser(
    const fs_path &path, const char *delimiters, const char *comentaries, const char *quotes)
{
    if (!fs_path_empty(path))
    {
        m_File.Load(path);
    }
    StartupInitalize(delimiters, comentaries, quotes);
}

TextFileParser::~TextFileParser()
{
    m_File.Unload();
}

void TextFileParser::StartupInitalize(
    const char *delimiters, const char *comentaries, const char *quotes)
{
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

void TextFileParser::Restart()
{
    m_File.ResetPtr();
}

bool TextFileParser::IsEOF()
{
    return (m_File.Ptr >= m_End);
}

void TextFileParser::GetEOL()
{
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

bool TextFileParser::IsDelimiter()
{
    bool result = false;
    for (int i = 0; i < m_DelimitersSize && !result; i++)
    {
        result = (*m_Ptr == m_Delimiters[i]);
    }
    return result;
}

void TextFileParser::SkipToData()
{
    while (m_Ptr < m_EOL && (*m_Ptr != 0u) && IsDelimiter())
    {
        m_Ptr++;
    }
}

bool TextFileParser::IsComment()
{
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

bool TextFileParser::IsQuote()
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

bool TextFileParser::IsSecondQuote()
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

astr_t TextFileParser::ObtainData()
{
    astr_t result;
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

astr_t TextFileParser::ObtainQuotedData()
{
    bool exit = false;
    astr_t result;
    for (int i = 0; i < m_QuotesSize; i += 2)
    {
        if (*m_Ptr == m_Quotes[i])
        {
            char endQuote = m_Quotes[i + 1];
            exit = true;
            m_Ptr++;
            uint8_t *ptr = m_Ptr;
            while (ptr < m_EOL && *ptr != 0u && *ptr != '\n' && *ptr != endQuote)
            {
                ptr++;
            }

            size_t size = ptr - m_Ptr;
            if (size > 0)
            {
                result.resize(size);
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

void TextFileParser::SaveRawLine()
{
    size_t size = m_EOL - m_Ptr;

    if (size > 0)
    {
        RawLine.resize(size, 0);
        memcpy(&RawLine[0], &m_Ptr[0], size);

        while (RawLine.length() != 0u && (RawLine[size - 1] == '\r' || RawLine[size - 1] == '\n'))
        {
            RawLine.resize(RawLine.length() - 1);
        }
    }
    else
    {
        RawLine = "";
    }
}

std::vector<astr_t> TextFileParser::ReadTokens(bool trim)
{
    m_Trim = trim;
    std::vector<astr_t> result;

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

            auto buf = ObtainQuotedData();
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

std::vector<astr_t> TextFileParser::GetTokens(const astr_t &str, bool trim)
{
    m_Trim = trim;
    std::vector<astr_t> result;

    uint8_t *oldEnd = m_End;
    m_Ptr = (uint8_t *)str.c_str();
    m_End = m_Ptr + str.size();
    m_EOL = m_End;

    SaveRawLine();
    while (m_Ptr < m_EOL)
    {
        SkipToData();
        if (IsComment())
        {
            break;
        }
        auto buf = ObtainQuotedData();
        if (buf.length() != 0u)
        {
            result.push_back(buf);
        }
    }

    m_End = oldEnd;
    return result;
}
