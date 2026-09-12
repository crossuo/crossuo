// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/finditem.cpp): QString trimming/parsing
// (trimmed/at/remove) became std::string equivalents; CAssistant::TextToSerial/
// TextToGraphic come from core/textconv.h (temporary home).

#include "finditem.h"
#include "core/textconv.h"
#include "core/textparser.h"
#include <cctype>

namespace xuoassist {

static string qtrimmed(const string &str)
{
    size_t begin = 0;
    size_t end = str.length();
    while (begin < end && std::isspace((unsigned char)str[begin]))
    {
        begin++;
    }
    while (end > begin && std::isspace((unsigned char)str[end - 1]))
    {
        end--;
    }
    return str.substr(begin, end - begin);
}

CFindItem::CFindItem()
{
}

CFindItem::CFindItem(const string &graphic, const string &color)
    : m_Graphic(graphic)
    , m_Color(color)
{
    OnChangeGraphic(graphic);
    OnChangeColor(color);
}

CFindItem::~CFindItem()
{
}

void CFindItem::OnChangeGraphic(const string &val)
{
    CTextParser graphicParser(val, "|", "", "");

    vector<string> listGraphic = graphicParser.ReadTokens();

    m_ListGraphic.clear();
    m_ListGraphicFlags.clear();
    m_IgnoreGraphics.clear();

    if (!listGraphic.empty())
    {
        for (const string &strGraphic : listGraphic)
        {
            uint currentGraphic = 0;
            uint flagsGraphic = ParseToken(strGraphic, currentGraphic, true);

            if ((flagsGraphic & FTS_NOT_EQUAL) != 0u)
            {
                m_IgnoreGraphics.push_back(currentGraphic);
            }
            else
            {
                m_ListGraphic.push_back(currentGraphic);
                m_ListGraphicFlags.push_back(flagsGraphic);
            }
        }

        if (m_ListGraphic.empty())
        {
            m_ListGraphic.push_back(0xFFFF);
            m_ListGraphicFlags.push_back(0);
        }
    }
}

void CFindItem::OnChangeColor(const string &val)
{
    CTextParser colorParser(val, "|", "", "");
    vector<string> listColor = colorParser.ReadTokens();

    m_ListColor.clear();
    m_ListColorFlags.clear();
    m_IgnoreColors.clear();

    if (!listColor.empty())
    {
        for (const string &strColor : listColor)
        {
            uint currentColor = 0;
            uint flagsColor = ParseToken(strColor, currentColor, true);

            if ((flagsColor & FTS_NOT_EQUAL) != 0u)
            {
                m_IgnoreColors.push_back(currentColor);
            }
            else
            {
                m_ListColor.push_back(currentColor);
                m_ListColorFlags.push_back(flagsColor);
            }
        }

        if (m_ListColor.empty())
        {
            m_ListColor.push_back(0xFFFF);
            m_ListColorFlags.push_back(0);
        }
    }
}

uint CFindItem::ParseToken(string str, uint &value, const bool &isGraphic) const
{
    str = qtrimmed(str);
    uint result = FTS_NORMAL;

    while (str.length() != 0)
    {
        if (str.at(0) == '!')
        {
            result |= FTS_NOT_EQUAL;
            str.erase(0, 1);
        }
        else if (str.at(0) == '<')
        {
            result |= FTS_LOWER;
            str.erase(0, 1);
        }
        else if (str.at(0) == '>')
        {
            result |= FTS_HIGHTER;
            str.erase(0, 1);
        }
        else
        {
            break;
        }
    }

    if (isGraphic)
    {
        value = TextToGraphic(str);
    }
    else
    {
        value = TextToSerial(str);
    }

    return result;
}

bool CFindItem::TestValues(const uint &original, const uint &test, const uint &flags) const
{
    if (test != 0xFFFF)
    {
        if (((flags & FTS_NOT_EQUAL) != 0u) && original == test)
        {
            return false;
        }
        if (((flags & FTS_LOWER) != 0u) && original >= test)
        {
            return false;
        }
        if (((flags & FTS_HIGHTER) != 0u) && original <= test)
        {
            return false;
        }
        if ((flags == 0u) && original != test)
        {
            return false;
        }
    }

    return true;
}

bool CFindItem::Found(const ushort &graphic, const ushort &color) const
{
    if (!m_IgnoreGraphics.empty())
    {
        for (const ushort &item : m_IgnoreGraphics)
        {
            if (item == graphic)
            {
                return false;
            }
        }
    }

    if (!m_IgnoreColors.empty())
    {
        for (const ushort &item : m_IgnoreColors)
        {
            if (item == color)
            {
                return false;
            }
        }
    }

    int graphicsCount = (int)m_ListGraphic.size();
    int colorsCount = (int)m_ListColor.size();

    if ((graphicsCount == 0) || (colorsCount == 0))
    {
        return false;
    }

    for (int i = 0; i < graphicsCount; i++)
    {
        if (TestValues(graphic, m_ListGraphic[i], m_ListGraphicFlags[i]))
        {
            for (int j = 0; j < colorsCount; j++)
            {
                if (TestValues(color, m_ListColor[j], m_ListColorFlags[j]))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

} // namespace xuoassist
