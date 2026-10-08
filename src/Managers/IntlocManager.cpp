// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2017 Hotride

#include <common/str.h>
#include "IntlocManager.h"
#include "ClilocManager.h"
#include <xuocore/uodata.h>

// FourCC tags stored big-endian in the intloc files (matches what gcc's
// implementation-defined multi-character constants produced before).
static constexpr uint32_t intloc_tag(char a, char b, char c, char d)
{
    return (uint32_t(a) << 24) | (uint32_t(b) << 16) | (uint32_t(c) << 8) | uint32_t(d);
}

CIntlocManager g_IntlocManager;

CIntloc::CIntloc(int fileIndex, const astr_t &lang)
{
    Loaded = false;
    Language = lang;
    FileIndex = fileIndex;
    if (Language.length() != 0u)
    {
        if (m_File.Load(UOFilePath("intloc%02i.%s", fileIndex, lang.c_str())))
        {
            while (!m_File.IsEOF())
            {
                uint32_t code = m_File.ReadUInt32BE();
                if (code == intloc_tag('T', 'E', 'X', 'T'))
                {
                    int len = m_File.ReadInt32BE();
                    // bounds are logical positions (platform-neutral cursor)
                    const size_t end = m_File.Pos() + size_t(len);
                    while (m_File.Pos() < end && !m_File.IsEOF())
                    {
                        m_Strings.push_back(wstr_from_utf8(m_File.ReadString()));
                    }
                }
                else if (code == intloc_tag('F', 'O', 'R', 'M'))
                {
                    m_File.Move(4);
                }
                else if (code == intloc_tag('I', 'N', 'F', 'O'))
                {
                    int len = m_File.ReadInt32BE();
                    m_File.Move(len + 1);
                }
                else if (
                    code == intloc_tag('D', 'A', 'T', 'A') || code == intloc_tag('L', 'A', 'N', 'G'))
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
    for (CIntloc *obj = (CIntloc *)m_Items; obj != nullptr; obj = (CIntloc *)obj->m_Next)
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

    for (CIntloc *obj2 = (CIntloc *)m_Items; obj2 != nullptr; obj2 = (CIntloc *)obj2->m_Next)
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
