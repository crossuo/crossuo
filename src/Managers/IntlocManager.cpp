// MIT License
// Copyright (C) October 2017 Hotride

#include <common/str.h>
#include "IntlocManager.h"
#include "ClilocManager.h"
#include "../Application.h"

CIntlocManager g_IntlocManager;

CIntloc::CIntloc(int fileIndex, const astr_t &lang)
{
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
                        m_Strings.push_back(wstr_from_utf8(m_File.ReadString()));
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
            Loaded = !m_Strings.empty();
        }
    }
}

CIntloc::~CIntloc()
{
    m_File.Unload();
    m_Strings.clear();
}

wstr_t CIntloc::Get(int id, bool toCamelCase)
{
    if (id < (int)m_Strings.size())
    {
        if (toCamelCase)
        {
            return wstr_camel_case(m_Strings[id]);
        }
        return m_Strings[id];
    }
    return {};
}

CIntloc *CIntlocManager::Intloc(int fileIndex, const astr_t &lang)
{
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

    QFOR(obj2, m_Items, CIntloc *)
    {
        if (obj2->Language == "enu" && obj2->FileIndex == fileIndex)
        {
            if (obj2->Loaded)
            {
                return obj2;
            }
            break;
        }
    }
    return nullptr;
}

wstr_t CIntlocManager::Intloc(const astr_t &lang, uint32_t clilocID, bool isNewCliloc) const
{
    assert(str_lower(lang) == lang);
    return g_ClilocManager.Cliloc(lang.empty() ? "enu" : lang)->GetW(clilocID, true);
}
