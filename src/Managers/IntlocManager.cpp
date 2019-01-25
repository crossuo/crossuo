// MIT License
// Copyright (C) October 2017 Hotride

#include "IntlocManager.h"
#include "ClilocManager.h"
#include "../Application.h"

CIntlocManager g_IntlocManager;

CIntloc::CIntloc(int fileIndex, const string &lang)
{
    DEBUG_TRACE_FUNCTION;
    Loaded = false;
    Language = lang;
    FileIndex = fileIndex;

    if (Language.length() != 0u)
    {
        if (m_File.Load(g_App.UOFilesPath("intloc%02i.%s", fileIndex, lang.c_str())))
        {
            while (!m_File.IsEOF())
            {
                uint32_t code = m_File.ReadUInt32BE();

                if (code == 'TEXT')
                {
                    int len = m_File.ReadInt32BE();

                    uint8_t *end = m_File.Ptr + len;

                    while (m_File.Ptr < end && !m_File.IsEOF())
                    {
                        m_Strings.push_back(DecodeUTF8(m_File.ReadString()));
                    }
                }
                else if (code == 'FORM')
                {
                    m_File.Move(4);
                }
                else if (code == 'INFO')
                {
                    int len = m_File.ReadInt32BE();
                    m_File.Move(len + 1);
                }
                else if (code == 'DATA' || code == 'LANG')
                {
                }
                else
                {
                    break;
                }
            }

            Loaded = (!m_Strings.empty());
        }
    }
}

CIntloc::~CIntloc()
{
    DEBUG_TRACE_FUNCTION;
    m_File.Unload();

    m_Strings.clear();
}

wstring CIntloc::Get(int id, bool toCamelCase)
{
    DEBUG_TRACE_FUNCTION;
    if (id < (int)m_Strings.size())
    {
        if (toCamelCase)
        {
            return ToCamelCaseW(m_Strings[id]);
        }

        return m_Strings[id];
    }

    return {};
}

CIntlocManager::CIntlocManager()

{
}

CIntlocManager::~CIntlocManager()
{
}

CIntloc *CIntlocManager::Intloc(int fileIndex, const string &lang)
{
    DEBUG_TRACE_FUNCTION;
    QFOR(obj, m_Items, CIntloc *)
    {
        if (obj->Language == lang && obj->FileIndex == fileIndex)
        {
            if (!obj->Loaded)
            {
                return nullptr;
            }

            return obj;
        }
    }

    CIntloc *obj = (CIntloc *)Add(new CIntloc(fileIndex, lang));

    if (obj->Loaded)
    {
        return obj;
    }

    QFOR(obj, m_Items, CIntloc *)
    {
        if (obj->Language == "enu" && obj->FileIndex == fileIndex)
        {
            if (obj->Loaded)
            {
                return obj;
            }

            break;
        }
    }

    return nullptr;
}

wstring CIntlocManager::Intloc(const string &lang, uint32_t clilocID, bool isNewCliloc)
{
    DEBUG_TRACE_FUNCTION;

    string language = ToLowerA(lang);
    if (language.length() == 0u)
    {
        language = "enu";
    }

    wstring str = {};

    if (str.length() == 0u)
    {
        str = g_ClilocManager.Cliloc(lang)->GetW(clilocID, true);
    }

    return str;
}
