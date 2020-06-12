// MIT License
// Copyright (C) August 2016 Hotride

#include "SpeechManager.h"
#include <xuocore/uodata.h>
#include <common/logging/logging.h>
#include <common/str.h>
#include <vector>
#include "../Config.h"
#include "../Misc.h"
#include "../Globals.h" // g_Language

CSpeechManager g_SpeechManager;

CSpeechItem::CSpeechItem(uint16_t code, const wstr_t &data)
    : Code(code)
    , Data(data)
{
    if ((Data.length() != 0u) && Data[Data.length() - 1] == L'*')
    {
        CheckEnd = true;
        Data.resize(Data.length() - 1);
    }

    if ((Data.length() != 0u) && Data[0] == L'*')
    {
        CheckStart = true;
        Data.erase(Data.begin());
    }
}

CSpeechManager::CSpeechManager()
{
}

CSpeechManager::~CSpeechManager()
{
    m_SpeechEntries.clear();
    m_LangCodes.clear();
}

bool CSpeechManager::LoadSpeech()
{
    LoadLangCodes();

    for (int i = 0; i < (int)m_LangCodes.size(); i++)
    {
        if (m_LangCodes[i].Abbreviature == g_Language)
        {
            CurrentLanguage = &m_LangCodes[i];
            break;
        }
    }

    if (CurrentLanguage == nullptr)
    {
        CurrentLanguage = &m_LangCodes[0];
        g_Language = str_lower(m_LangCodes[0].Abbreviature);
    }
    Info(Client, "selected language: %s", g_Language.c_str());

    CDataReader reader;
    std::vector<uint8_t> tempData;
    bool isUOP = false;
    if (g_FileManager.m_MainMisc.Start != nullptr)
    {
        auto block = g_FileManager.m_MainMisc.GetAsset(
            0x0891F809004D8081); // FIXME: find the string for this hash
        if (block != nullptr)
        {
            tempData = g_FileManager.m_MainMisc.GetData(block);
            reader.SetData(&tempData[0], tempData.size());
            isUOP = true;
        }
    }

    if (reader.Start == nullptr)
    {
        reader.SetData(g_FileManager.m_SpeechMul.Start, g_FileManager.m_SpeechMul.Size);
    }

    if (isUOP)
    {
        Info(Client, "loading speech from UOP");
        reader.Move(2);
        auto mainData = reader.ReadWStringLE(reader.Size - 2);
        std::vector<wstr_t> list;
        wstr_t temp;
        for (const auto &c : mainData)
        {
            if (c == 0x000D || c == 0x000A)
            {
                if (temp.length() != 0u)
                {
                    list.push_back(temp);
                    temp = {};
                }
            }
            else
            {
                temp.push_back(c);
            }
        }

        if (temp.length() != 0u)
        {
            list.push_back(temp);
            temp = {};
        }

        for (const wstr_t &line : list)
        {
            uint16_t code = 0xFFFF;
            temp = {};
            for (const auto c : line)
            {
                if (c == 0x0009)
                {
                    if (temp.length() != 0u)
                    {
                        code = str_to_int(temp);
                        temp = {};
                    }
                }
                else
                {
                    temp.push_back(c);
                }
            }

            if ((temp.length() != 0u) && code != 0xFFFF)
            {
                m_SpeechEntries.push_back(CSpeechItem(code, temp));
            }
        }
    }
    else
    {
        Info(Client, "loading speech from MUL");
        while (!reader.IsEOF())
        {
            const uint16_t code = reader.ReadUInt16BE();
            const int len = reader.ReadUInt16BE();
            if (len == 0)
            {
                continue;
            }

            auto str = wstr_from_utf8(reader.ReadString(len));
            m_SpeechEntries.push_back(CSpeechItem(code, str));
        }
    }

    Info(Client, "m_SpeechEntries.size()=%zi", m_SpeechEntries.size());
    m_Loaded = true;
    return true;
}

bool CSpeechManager::LoadLangCodes()
{
    m_LangCodes.push_back(CLangCode("enu", 101, "English", "United States"));
    CMappedFile &file = g_FileManager.m_LangcodeIff;

    file.ReadString(36);
    while (!file.IsEOF())
    {
        CLangCode langCodeData;
        file.Move(4);

        const uint32_t _entryLen = file.ReadUInt32BE();
        (void)_entryLen;
        langCodeData.Abbreviature = file.ReadString();
        langCodeData.Code = file.ReadUInt32LE();
        langCodeData.Language = file.ReadString();
        langCodeData.Country = file.ReadString();
        if (((langCodeData.Language.length() + langCodeData.Country.length() + 2) % 2) != 0u)
        {
            int nullTerminator = file.ReadUInt8();
            (void)nullTerminator;
            assert(
                nullTerminator == 0 &&
                "speechManager @ 138, invalid null terminator in langcodes.iff");
        }

        m_LangCodes.push_back(langCodeData);
        TRACE(Client, "[0x%04X]: %s", langCodeData.Code, langCodeData.Abbreviature.c_str());
    }

    //if (m_LangCodes.size() != 135)
    //	return false;

    return true;
}

void CSpeechManager::GetKeywords(const wstr_t &text, std::vector<uint32_t> &codes) const
{
    if (!m_Loaded || g_Config.ProtocolClientVersion < CV_305D)
    {
        return; // But in fact from the client version 2.0.7
    }

    const auto size = (int)m_SpeechEntries.size();
    auto copy = text;
    auto input = wstr_lower(copy);
    for (int i = 0; i < size; i++)
    {
        CSpeechItem entry = m_SpeechEntries[i];
        auto data = entry.Data;
        if (data.length() > input.length() || data.length() == 0)
        {
            continue;
        }

        if (!entry.CheckStart)
        {
            auto start = input.substr(0, data.length());
            size_t hits = start.find(data);
            if (hits == wstr_t::npos)
            {
                continue;
            }
        }

        if (!entry.CheckEnd)
        {
            auto end = input.substr(input.length() - data.length());
            size_t hits = end.find(data);
            if (hits == wstr_t::npos)
            {
                continue;
            }
        }

        size_t hits = input.find(data);
        if (hits != wstr_t::npos)
        {
            codes.push_back(entry.Code);
        }
    }
}
