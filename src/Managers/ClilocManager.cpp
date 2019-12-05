// MIT License
// Copyright (C) August 2016 Hotride

#include <algorithm>
#include <common/str.h>
#include "ClilocManager.h"
#include "../Application.h"
#include "../Globals.h" // g_Language

CClilocManager g_ClilocManager;

CCliloc::CCliloc(const std::string &lang)
{
    Loaded = false;
    Language = lang;
    if (Language.length() != 0u)
    {
        auto file = std::string("Cliloc.") + lang;
        auto path = g_App.UOFilesPath(file);
        if (m_File.Load(path))
        {
            Loaded = true;
        }
    }
}

CCliloc::~CCliloc()
{
    m_File.Unload();
    m_ClilocSystem.clear();
    m_ClilocRegular.clear();
    m_ClilocSupport.clear();
}

std::string CCliloc::Load(uint32_t &id)
{
    std::string result;
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
                }

                if (id >= 3000000)
                {
                    m_ClilocSupport[currentID] = result;
                }
                else if (id >= 1000000)
                {
                    m_ClilocRegular[currentID] = result;
                }
                else
                {
                    m_ClilocSystem[currentID] = result;
                }

                return result;
            }

            m_File.Move(len);
        }
    }
    id = 0;
    return result;
}

std::wstring CCliloc::CamelCaseTest(bool toCamelCase, const std::string &result)
{
    if (toCamelCase)
    {
        return ToCamelCaseW(DecodeUTF8(result));
    }

    return DecodeUTF8(result);
}

std::wstring CCliloc::GetX(int id, bool toCamelCase, std::string &result)
{
    if (id >= 3000000)
    {
        CLILOC_MAP::iterator i = m_ClilocSupport.find(id);
        if (i != m_ClilocSupport.end() && ((*i).second.length() != 0u))
        {
            return CamelCaseTest(toCamelCase, (*i).second);
        }
    }
    else if (id >= 1000000)
    {
        CLILOC_MAP::iterator i = m_ClilocRegular.find(id);
        if (i != m_ClilocRegular.end() && ((*i).second.length() != 0u))
        {
            return CamelCaseTest(toCamelCase, (*i).second);
        }
    }
    else
    {
        CLILOC_MAP::iterator i = m_ClilocSystem.find(id);
        if (i != m_ClilocSystem.end() && ((*i).second.length() != 0u))
        {
            return CamelCaseTest(toCamelCase, (*i).second);
        }
    }

    uint32_t tmpID = id;
    auto loadStr = Load(tmpID);
    if (loadStr.length() != 0u)
    {
        return CamelCaseTest(toCamelCase, loadStr);
    }

    if (tmpID == id && (loadStr.length() == 0u))
    {
        return {};
    }

    if (Language != "ENU" && this->Language != "enu")
    {
        return g_ClilocManager.Cliloc("enu")->GetW(id, toCamelCase, result);
    }

    if (result.length() == 0u)
    {
        char str[50]{};
        sprintf(str, "Unknown Cliloc #%i", id);
        result = str;
    }

    return CamelCaseTest(toCamelCase, result);
}

std::string CCliloc::GetA(int id, bool toCamelCase, std::string result)
{
    return ToString(GetX(id, toCamelCase, result));
}

std::wstring CCliloc::GetW(int id, bool toCamelCase, std::string result)
{
    return GetX(id, toCamelCase, result);
}

CClilocManager::CClilocManager()
{
}

CClilocManager::~CClilocManager()
{
    m_ENUCliloc = nullptr;
    m_LastCliloc = nullptr;
}

CCliloc *CClilocManager::Cliloc(const std::string &lang)
{
    auto language = ToLowerA(lang);
    if (language.length() == 0u)
    {
        language = "enu";
    }

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

    CCliloc *obj = (CCliloc *)Add(new CCliloc(language));
    if (!obj->Loaded)
    {
        return Cliloc("enu");
    }

    m_LastCliloc = obj;
    return obj;
}

std::wstring
CClilocManager::ParseArgumentsToClilocString(int cliloc, bool toCamelCase, std::wstring args)
{
    while ((args.length() != 0u) && args[0] == L'\t')
    {
        args.erase(args.begin());
    }

    return ParseArgumentsToCliloc(cliloc, toCamelCase, args);
}

std::wstring
CClilocManager::ParseXmfHtmlArgumentsToCliloc(int cliloc, bool toCamelCase, std::wstring args)
{
    while ((args.length() != 0u) && args[0] == L'@')
    {
        args.erase(std::remove(args.begin(), args.end(), L'@'), args.end());
    }

    return ParseArgumentsToCliloc(cliloc, toCamelCase, args);
}

std::wstring CClilocManager::ParseArgumentsToCliloc(int cliloc, bool toCamelCase, std::wstring args)
{
    auto message = Cliloc(g_Language)->GetW(cliloc, toCamelCase);
    std::vector<std::wstring> arguments;
    while (true)
    {
        size_t pos = args.find(L'\t');
        if (pos != std::string::npos)
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
        if (pos1 == std::string::npos)
        {
            break;
        }

        size_t pos2 = message.find(L'~', pos1 + 1);
        if (pos2 == std::string::npos)
        {
            break;
        }

        if (arguments[i].length() > 1 && *arguments[i].c_str() == L'#')
        {
            uint32_t id = std::stoi(arguments[i].c_str() + 1);
            arguments[i] = Cliloc(g_Language)->GetW(id, toCamelCase);
        }

        message.replace(pos1, pos2 - pos1 + 1, arguments[i]);
    }

    if (toCamelCase)
    {
        return ToCamelCaseW(message);
    }

    return message;
}
