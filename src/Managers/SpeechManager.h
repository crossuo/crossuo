// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../IndexObject.h"

class CLangCode
{
public:
    int Code = 0;
    uint32_t Unknown = 0;
    string Abbreviature;
    string Language;
    string Country;

    CLangCode() {}
    CLangCode(const string &abbreviature, int code, const string &language, const string &country)
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
    wstring Data;

    bool CheckStart = false;
    bool CheckEnd = false;

    CSpeechItem() {}
    CSpeechItem(uint16_t code, const wstring &data);
    virtual ~CSpeechItem() {}
};

class CSpeechManager
{
    CLangCode *CurrentLanguage = nullptr;

private:
    vector<CSpeechItem> m_SpeechEntries;
    vector<CLangCode> m_LangCodes;
    bool m_Loaded = false;

public:
    CSpeechManager();
    ~CSpeechManager();

    bool LoadSpeech();
    bool LoadLangCodes();
    void GetKeywords(const wchar_t *text, vector<uint32_t> &codes);
};

extern CSpeechManager g_SpeechManager;
