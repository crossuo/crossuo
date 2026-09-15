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
    : Type(type)
    , Text(text)
    , Graphic(graphic)
    , Hue(hue)
    , MinColor(minColor)
    , MidColor(midColor)
    , Color(color)
    , Background(background)
    , BackgroundMinColor(backgroundMinColor)
    , BackgroundMidColor(backgroundMidColor)
    , BackgroundColor(backgroundColor)
    , MinValue(minValue)
    , MidValue(midValue)
    , Percents(percents)
{
}

CDisplayItem::CDisplayItem(const CDisplayItem &obj)
    : Type(obj.Type)
    , Text(obj.Text)
    , Graphic(obj.Graphic)
    , Hue(obj.Hue)
    , MinColor(obj.MinColor)
    , MidColor(obj.MidColor)
    , Color(obj.Color)
    , Background(obj.Background)
    , BackgroundMinColor(obj.BackgroundMinColor)
    , BackgroundMidColor(obj.BackgroundMidColor)
    , BackgroundColor(obj.BackgroundColor)
    , MinValue(obj.MinValue)
    , MidValue(obj.MidValue)
    , Percents(obj.Percents)
{
}

CDisplayItem &CDisplayItem::operator=(const CDisplayItem &obj)
{
    if (this != &obj)
    {
        Type = obj.Type;
        Text = obj.Text;
        Graphic = obj.Graphic;
        Hue = obj.Hue;
        MinColor = obj.MinColor;
        MidColor = obj.MidColor;
        Color = obj.Color;
        Background = obj.Background;
        BackgroundMinColor = obj.BackgroundMinColor;
        BackgroundMidColor = obj.BackgroundMidColor;
        BackgroundColor = obj.BackgroundColor;
        MinValue = obj.MinValue;
        MidValue = obj.MidValue;
        Percents = obj.Percents;
    }
    return *this;
}

} // namespace xuoassist
