// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <vector>
#include <stdint.h>
#include <common/str.h>

class CLangCode
{
public:
    int Code = 0;
    uint32_t Unknown = 0;
    astr_t Abbreviature;
    astr_t Language;
    astr_t Country;

    CLangCode() {}
    CLangCode(const astr_t &abbreviature, int code, const astr_t &language, const astr_t &country)
        : Code(code)
        , Unknown(0)
        , Abbreviature(abbreviature)
        , Language(language)
        , Country(country)
    {
    }
    virtual ~CLangCode() {}
};

class CSpeechItem
{
public:
    uint16_t Code = 0;
    wstr_t Data;

    bool CheckStart = false;
    bool CheckEnd = false;

    CSpeechItem() {}
    CSpeechItem(uint16_t code, const wstr_t &data);
    virtual ~CSpeechItem() {}
};

class CSpeechManager
{
    CLangCode *CurrentLanguage = nullptr;

private:
    std::vector<CSpeechItem> m_SpeechEntries;
    std::vector<CLangCode> m_LangCodes;
    bool m_Loaded = false;

public:
    CSpeechManager();
    ~CSpeechManager();

    bool LoadSpeech();
    bool LoadLangCodes();
    void GetKeywords(const wstr_t &text, std::vector<uint32_t> &codes) const;
};

extern CSpeechManager g_SpeechManager;
