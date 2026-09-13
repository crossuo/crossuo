// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/textparser.cpp), token-for-token identical.

#include "textparser.h"
#include <cstring>
#include <cstdlib>

namespace xuoassist {

CTextParser::CTextParser(
    const string &text, const char *delimiters, const char *comentaries, const char *quotes,
    const bool &saveQuotes)
    : m_RawLine("")
    , m_SaveQuotes(saveQuotes)
    , m_Text(text)
    , m_Ptr(nullptr)
    , m_StartPtr(nullptr)
    , m_EOL(nullptr)
    , m_File((uchar *)m_Text.c_str(), m_Text.length())
{
    // Initialize the delimiters
    memset(&m_Delimiters[0], 0, sizeof(m_Delimiters));
    m_DelimitersSize = strlen(delimiters);

    if (m_DelimitersSize != 0)
    {
        memcpy(&m_Delimiters[0], &delimiters[0], m_DelimitersSize);
    }

    // Initialize the comments
    memset(&m_Comentaries[0], 0, sizeof(m_Comentaries));
    m_ComentariesSize = strlen(comentaries);

    if (m_ComentariesSize != 0)
    {
        memcpy(&m_Comentaries[0], &comentaries[0], m_ComentariesSize);
    }

    // Initialize the quotes
    memset(&m_Quotes[0], 0, sizeof(m_Quotes));
    m_QuotesSize = strlen(quotes);

    if (m_QuotesSize != 0)
    {
        memcpy(&m_Quotes[0], &quotes[0], m_QuotesSize);
    }

    // Compute the end of file
    m_End = m_File.End;
}

CTextParser::~CTextParser()
{
}

// Reset the pointer to the start
void CTextParser::Restart()
{
    m_File.ResetPtr();
}

// Check for end of file
bool CTextParser::IsEOF()
{
    return (m_File.Ptr >= m_End);
}

// Get the end of line
void CTextParser::GetEOL()
{
    // The EOL starts at the current address
    m_EOL = m_File.Ptr;

    // If the file has not been read to the end
    if (!IsEOF())
    {
        // Look for the end of line
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

// Check for a delimiter
bool CTextParser::IsDelimiter()
{
    bool result = false;

    // Walk over all delimiters
    for (int i = 0; i < m_DelimitersSize && !result; i++)
    {
        result = (*m_Ptr == m_Delimiters[i]);
    }

    return result;
}

// Skip everything up to the data
void CTextParser::SkipToData()
{
    // While the character is a delimiter, walk the delimiter set and advance the pointer
    while (m_Ptr < m_EOL && (*m_Ptr != 0u) && IsDelimiter())
    {
        m_Ptr++;
    }
}

// Check for a comment
bool CTextParser::IsComment()
{
    bool result = (*m_Ptr == '\n');

    // Walk over all comment markers
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

// Check for a quote
bool CTextParser::IsQuote()
{
    bool result = (*m_Ptr == '\n');

    // Walk over the quote set, pair format: openQuote, closeQuote (they may be identical)
    for (int i = 0; i < m_QuotesSize; i += 2)
    {
        // If a quote matched
        if (*m_Ptr == m_Quotes[i] || *m_Ptr == m_Quotes[i + 1])
        {
            result = true;
            break;
        }
    }

    return result;
}

// Check for a closing quote
bool CTextParser::IsSecondQuote()
{
    bool result = (*m_Ptr == '\n');

    // Walk over the quote set, pair format: openQuote, closeQuote (they may be identical)
    for (int i = 0; i < m_QuotesSize; i += 2)
    {
        // If a quote matched
        if (*m_Ptr == m_Quotes[i + 1])
        {
            result = true;
            break;
        }
    }

    return result;
}

// Get the next token
string CTextParser::ObtainData()
{
    string result = "";

    // While the character is valid (not a stopper) - append the data and advance the pointer
    while (m_Ptr < m_End && (*m_Ptr != 0u) && *m_Ptr != '\n')
    {
        // Check for a delimiter
        if (IsDelimiter() || IsQuote())
        {
            break;
            // Check for a comment
        }
        if (IsComment())
        {
            m_Ptr = m_EOL;
            break;
        }

        // Handle line breaks and trim spaces if requested
        if (*m_Ptr != '\r' && (!m_Trim || (*m_Ptr != ' ' && *m_Ptr != '\t')))
        {
            result += *m_Ptr;
        }

        m_Ptr++;
    }

    return result;
}

// Get the next quoted string fragment, or a token if no quotes are found
string CTextParser::ObtainQuotedData()
{
    bool exit = false;
    string result;

    // Walk over the quote set, pair format: openQuote, closeQuote (they may be identical)
    for (int i = 0; i < m_QuotesSize; i += 2)
    {
        // If a quote matched
        if (*m_Ptr == m_Quotes[i])
        {
            // Remember the closing quote
            char endQuote = m_Quotes[i + 1];
            exit = true;

            // Advance the pointer, the opening quote is not part of the result
            m_Ptr++;
            uchar *ptr = m_Ptr;

            // Skip everything up to the end of line or the closing quote
            while (ptr < m_EOL && (*ptr != 0u) && *ptr != '\n' && *ptr != endQuote)
            {
                ptr++;
            }

            // Fragment size
            int size = ptr - m_Ptr;

            if (size > 0)
            {
                // Allocate space for the fragment
                result.resize(size + 1);

                // Copy the fragment
                memcpy(&result[0], &m_Ptr[0], size);
                result[size] = 0;

                // Walk the fragment from the end and strip the trailing junk
                for (int j = size - 1; j >= 0 && result[j] == '\r'; j--)
                {
                    result[j] = 0;
                }

                // Pointer to the end of the fragment
                m_Ptr = ptr;

                if (m_Ptr < m_EOL && *m_Ptr == endQuote)
                {
                    m_Ptr++;
                }
            }

            if (m_SaveQuotes)
            {
                result.insert(result.begin(), m_Quotes[i]);
                result = string(result) + endQuote;
            }

            break;
        }
    }

    // If this is not quoted - extract a word
    if (!exit)
    {
        return ObtainData();
    }

    return result;
}

// Read the tokens from the file
void CTextParser::SaveRawLine()
{
    int size = m_EOL - m_Ptr;

    if (size > 0)
    {
        m_RawLine.resize(size, 0);
        memcpy(&m_RawLine[0], &m_Ptr[0], size);

        while ((m_RawLine.length() != 0u) &&
               (m_RawLine[size - 1] == '\r' || m_RawLine[size - 1] == '\n'))
        {
            m_RawLine.resize(m_RawLine.length() - 1);
        }
    }
    else
    {
        m_RawLine = "";
    }
}

void CTextParser::SavePosition()
{
    m_StartPtr = m_File.Ptr;
}

void CTextParser::LoadPosition()
{
    m_File.Ptr = m_StartPtr;
}

// Read the tokens from the file
vector<string> CTextParser::ReadTokens(bool trim)
{
    m_Trim = trim;
    vector<string> result;

    // If the end of the file has not been reached
    if (!IsEOF())
    {
        // Initialize the pointers
        m_Ptr = m_File.Ptr;
        GetEOL();

        SaveRawLine();

        // Walk until the end of line
        while (m_Ptr < m_EOL)
        {
            // Skip the delimiters
            SkipToData();

            // A comment - stop here
            if (IsComment())
            {
                break;
            }

            // Get the token
            string buf = ObtainQuotedData();

            // If the token is not empty - push it onto the stack
            if (buf.length() != 0)
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

// Read the tokens from a string
vector<string> CTextParser::GetTokens(const char *str, bool trim)
{
    m_Trim = trim;
    vector<string> result;

    // Save the previous end-of-data value
    uchar *oldEnd = m_End;

    m_Ptr = (uchar *)str;
    m_End = (uchar *)str + strlen(str);
    m_EOL = m_End;

    SaveRawLine();

    // Walk the string
    while (m_Ptr < m_EOL)
    {
        // Skip the delimiters
        SkipToData();

        // A comment - stop here
        if (IsComment())
        {
            break;
        }

        // Get the token
        string buf = ObtainQuotedData();

        // If the token is not empty - push it onto the stack
        if (buf.length() != 0)
        {
            result.push_back(buf);
        }
        else if (IsSecondQuote())
        {
            m_Ptr++;
        }
    }

    // Restore the previous end-of-data value
    m_End = oldEnd;

    return result;
}

} // namespace xuoassist
