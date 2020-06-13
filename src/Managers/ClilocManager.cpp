// MIT License
// Copyright (C) August 2016 Hotride

#include <algorithm>
#include <common/str.h>
#include "ClilocManager.h"
#include "../Application.h"
#include "../Globals.h" // g_Language

CClilocManager g_ClilocManager;

// System (id < 1000000)
// Regular (id >= 1000000 && id < 3000000)
// Support (id >= 3000000)
typedef std::map<uint32_t, astr_t> ClilocMap;
static ClilocMap s_Cache;

#define CONVERT_CASE(b, x) b ? str_camel_case(x) : x

CCliloc::CCliloc(const astr_t &lang)
{
    Loaded = false;
    Language = lang;
    if (Language.length() != 0)
    {
        auto file = astr_t("Cliloc.") + lang;
        auto path = g_App.UOFilesPath(file);
        Loaded = m_File.Load(path);
    }
}

CCliloc::~CCliloc()
{
    m_File.Unload();
}

astr_t CCliloc::Load(uint32_t id)
{
    astr_t result;
    if (Loaded)
    {
        m_File.ResetPtr();
        m_File.Move(6);
        while (!m_File.IsEOF())
        {
            auto currentID = m_File.ReadUInt32LE();
            m_File.Move(1);
            short len = m_File.ReadUInt16LE();
            if (currentID == id)
            {
                if (len > 0)
                {
                    result = m_File.ReadString(len);
                    s_Cache[currentID] = result;
                }
                return result;
            }

            m_File.Move(len);
        }
    }
    return result;
}

astr_t CCliloc::GetX(int id, bool toCamelCase, const char *default_value)
{
    auto it = s_Cache.find(id);
    if (it != s_Cache.end() && !(*it).second.empty())
    {
        return CONVERT_CASE(toCamelCase, (*it).second);
    }

    astr_t result = Load(id);
    if (Language != "enu")
    {
        result = g_ClilocManager.Cliloc("enu")->GetX(id, toCamelCase, default_value);
    }

    if (result.length() == 0)
    {
        if (default_value == nullptr)
        {
            char str[50];
            snprintf(str, sizeof(str), "Unknown Cliloc #%i", id);
            result = str;
        }
        else
        {
            result = default_value;
        }
    }
    return CONVERT_CASE(toCamelCase, result);
}

astr_t CCliloc::GetA(int id, bool toCamelCase, const char *default_value)
{
    return GetX(id, toCamelCase, default_value);
}

wstr_t CCliloc::GetW(int id, bool toCamelCase, const char *default_value)
{
    return wstr_from_utf8(GetX(id, toCamelCase, default_value));
}

CClilocManager::~CClilocManager()
{
    m_ENUCliloc = nullptr;
    m_LastCliloc = nullptr;
}

CCliloc *CClilocManager::Cliloc(const astr_t &lang)
{
    assert(str_lower(lang) == lang);
    auto language = lang.length() == 0 ? "enu" : lang;
    if (language == "enu")
    {
        if (m_ENUCliloc == nullptr)
        {
            m_ENUCliloc = (CCliloc *)Add(new CCliloc(language));
        }
        return m_ENUCliloc;
    }

    if (m_LastCliloc != nullptr && m_LastCliloc->Language == language)
    {
        if (!m_LastCliloc->Loaded)
        {
            return m_ENUCliloc;
        }
        return m_LastCliloc;
    }

    QFOR(obj, m_Items, CCliloc *)
    {
        if (obj->Language == language)
        {
            if (!obj->Loaded)
            {
                return m_ENUCliloc;
            }
            m_LastCliloc = obj;
            return obj;
        }
    }

    auto obj = (CCliloc *)Add(new CCliloc(language));
    if (!obj->Loaded)
    {
        return Cliloc("enu");
    }

    m_LastCliloc = obj;
    return obj;
}

wstr_t CClilocManager::ParseArgumentsToClilocString(int cliloc, bool toCamelCase, wstr_t args)
{
    while (args.length() != 0 && args[0] == L'\t')
    {
        args.erase(args.begin());
    }
    return ParseArgumentsToCliloc(cliloc, toCamelCase, args);
}

wstr_t CClilocManager::ParseXmfHtmlArgumentsToCliloc(int cliloc, bool toCamelCase, wstr_t args)
{
    while (args.length() != 0 && args[0] == L'@')
    {
        args.erase(std::remove(args.begin(), args.end(), L'@'), args.end());
    }
    return ParseArgumentsToCliloc(cliloc, toCamelCase, args);
}

wstr_t CClilocManager::ParseArgumentsToCliloc(int cliloc, bool toCamelCase, wstr_t args)
{
    auto message = Cliloc(g_Language)->GetW(cliloc, toCamelCase);
    std::vector<wstr_t> arguments;
    while (true)
    {
        size_t pos = args.find(L'\t');
        if (pos != astr_t::npos)
        {
            arguments.push_back(args.substr(0, pos));
            args = args.substr(pos + 1);
        }
        else
        {
            arguments.push_back(args);
            break;
        }
    }

    for (int i = 0; i < (int)arguments.size(); i++)
    {
        size_t pos1 = message.find(L'~');
        if (pos1 == astr_t::npos)
        {
            break;
        }

        size_t pos2 = message.find(L'~', pos1 + 1);
        if (pos2 == astr_t::npos)
        {
            break;
        }

        if (arguments[i].length() > 1 && *arguments[i].c_str() == L'#')
        {
            uint32_t id = str_to_int(arguments[i].c_str() + 1);
            arguments[i] = Cliloc(g_Language)->GetW(id, toCamelCase);
        }
        message.replace(pos1, pos2 - pos1 + 1, arguments[i]);
    }

    if (toCamelCase)
    {
        return wstr_camel_case(message);
    }
    return message;
}
