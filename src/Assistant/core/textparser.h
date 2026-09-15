// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/textparser.h): in-memory token parser for
// cfg-style text. Strings are std::string, string lists are std::vector<std::string>.

#pragma once

#include "global.h"
#include <xuocore/mappedfile.h> // ::CDataReader (the client data reader)

namespace xuoassist {

class CTextParser
{
public:
    string RawLine = "";
    bool SaveQuotes = false;

private:
    // Loaded text
    string m_Text;

    // Pointer to the character being processed
    uchar *m_Ptr;

    // Pointer to the start of the previously processed line
    uchar *m_StartPtr;

    // Address of the end of line
    uchar *m_EOL;

    // Address of the end of file
    uchar *m_End;

    CDataReader m_File;

    // Delimiters
    char m_Delimiters[50];

    // Delimiter count
    int m_DelimitersSize;

    // Comment markers
    char m_Comentaries[50];

    // Comment marker count
    int m_ComentariesSize;

    // Quote set, pair format: openQuote, closeQuote (they may be identical)
    char m_Quotes[50];

    // Number of entries in the quote set
    int m_QuotesSize;

    // Trim strings on output
    bool m_Trim{ true };

    // Get the end of line
    void GetEOL();

    // Check for a delimiter
    bool IsDelimiter();

    // Skip everything up to the data
    void SkipToData();

    // Check for a comment
    bool IsComment();

    // Check for a quote
    bool IsQuote();

    // Check for a closing quote
    bool IsSecondQuote();

    // Get the next token
    string ObtainData();

    // Get the next quoted string fragment, or a token if no quotes are found
    string ObtainQuotedData();

    void SaveRawLine();

public:
    CTextParser(
        const string &text,
        const char *delimiters = "",
        const char *comentaries = "",
        const char *quotes = "",
        const bool &saveQuotes = false);
    ~CTextParser();

    // Reset the pointer to the start
    void Restart();

    // Check for end of file
    bool IsEOF();

    void SavePosition();

    void LoadPosition();

    // Read the tokens from the file
    vector<string> ReadTokens(bool trim = true);

    // Read the tokens from a string
    vector<string> GetTokens(const char *str, bool trim = true);
};

} // namespace xuoassist

