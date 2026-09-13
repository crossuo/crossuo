// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/displayitem.cpp), verbatim.

#include "displayitem.h"

namespace xuoassist {

CDisplayItem::CDisplayItem(
    const DISPLAY_ITEM_TYPE &type,
    string text,
    ushort graphic,
    ushort hue,
    uint minColor,
    uint midColor,
    uint color,
    bool background,
    uint backgroundMinColor,
    uint backgroundMidColor,
    uint backgroundColor,
    int minValue,
    int midValue,
    bool percents)
    : m_Type(type)
    , m_Text(text)
    , m_Graphic(graphic)
    , m_Hue(hue)
    , m_MinColor(minColor)
    , m_MidColor(midColor)
    , m_Color(color)
    , m_Background(background)
    , m_BackgroundMinColor(backgroundMinColor)
    , m_BackgroundMidColor(backgroundMidColor)
    , m_BackgroundColor(backgroundColor)
    , m_MinValue(minValue)
    , m_MidValue(midValue)
    , m_Percents(percents)
{
}

CDisplayItem::CDisplayItem(const CDisplayItem &obj)
    : m_Type(obj.GetType())
    , m_Text(obj.GetText())
    , m_Graphic(obj.GetGraphic())
    , m_Hue(obj.GetHue())
    , m_MinColor(obj.GetMinColor())
    , m_MidColor(obj.GetMidColor())
    , m_Color(obj.GetColor())
    , m_Background(obj.GetBackground())
    , m_BackgroundMinColor(obj.GetBackgroundMinColor())
    , m_BackgroundMidColor(obj.GetBackgroundMidColor())
    , m_BackgroundColor(obj.GetBackgroundColor())
    , m_MinValue(obj.GetMinValue())
    , m_MidValue(obj.GetMidValue())
    , m_Percents(obj.GetPercents())
{
}

CDisplayItem &CDisplayItem::operator=(const CDisplayItem &obj)
{
    if (this != &obj)
    {
        m_Type = obj.m_Type;
        m_Text = obj.m_Text;
        m_Graphic = obj.m_Graphic;
        m_Hue = obj.m_Hue;
        m_MinColor = obj.m_MinColor;
        m_MidColor = obj.m_MidColor;
        m_Color = obj.m_Color;
        m_Background = obj.m_Background;
        m_BackgroundMinColor = obj.m_BackgroundMinColor;
        m_BackgroundMidColor = obj.m_BackgroundMidColor;
        m_BackgroundColor = obj.m_BackgroundColor;
        m_MinValue = obj.m_MinValue;
        m_MidValue = obj.m_MidValue;
        m_Percents = obj.m_Percents;
    }
    return *this;
}

} // namespace xuoassist
