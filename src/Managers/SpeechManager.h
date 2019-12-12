// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <vector>
#include <stdint.h>
#include <common/str.h>

class CLangCode
{
public:
    int Code = 0;
    uint32_t Unknown = 0;
    std::string Abbreviature;
    std::string Language;
    std::string Country;

    CLangCode() {}
    CLangCode(
        const std::string &abbreviature,
        int code,
        const std::string &language,
        const std::string &country)
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
    std::wstring Data;

    bool CheckStart = false;
    bool CheckEnd = false;

    CSpeechItem() {}
    CSpeechItem(uint16_t code, const std::wstring &data);
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
    void GetKeywords(const wchar_t *text, std::vector<uint32_t> &codes);
};

extern CSpeechManager g_SpeechManager;
