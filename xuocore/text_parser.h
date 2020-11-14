// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <stdint.h>
#include <common/str.h>
#include <common/fs.h>
#include <xuocore/mappedfile.h>

class TextFileParser
{
public:
    astr_t RawLine = "";

private:
    CMappedFile m_File;
    uint8_t *m_Ptr = 0;
    uint8_t *m_EOL = 0;
    uint8_t *m_End = 0;
    char m_Delimiters[50];
    int m_DelimitersSize = 0;
    char m_Comentaries[50];
    int m_ComentariesSize = 0;
    char m_Quotes[50];
    int m_QuotesSize = 0;
    bool m_Trim = false;

    void GetEOL();
    bool IsDelimiter();
    void SkipToData();
    bool IsComment();
    bool IsQuote();
    bool IsSecondQuote();
    astr_t ObtainData();
    astr_t ObtainQuotedData();
    void StartupInitalize(const char *delimiters, const char *comentaries, const char *quotes);
    void SaveRawLine();

public:
    TextFileParser(
        const fs_path &path,
        const char *delimiters = "",
        const char *comentaries = "",
        const char *quotes = "");
    ~TextFileParser();

    void Restart();
    bool IsEOF();

    std::vector<astr_t> ReadTokens(bool trim = true);
    std::vector<astr_t> GetTokens(const astr_t &str, bool trim = true);
};
