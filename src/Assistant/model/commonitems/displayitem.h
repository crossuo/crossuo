// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/displayitem.h): strings are astr_t.

#pragma once

#include "core/global.h"

namespace xuoassist {

enum DISPLAY_ITEM_TYPE
{
    DIT_NONE = 0,
    DIT_NAME,
    DIT_SERVER,
    DIT_STATUSBAR_SMALL,
    DIT_STATUSBAR_MIDDLE,
    DIT_STATUSBAR_LARGE,
    DIT_HITS,
    DIT_MAX_HITS,
    DIT_HITS_MAX_HITS,
    DIT_MANA,
    DIT_MAX_MANA,
    DIT_MANA_MAX_MANA,
    DIT_STAM,
    DIT_MAX_STAM,
    DIT_STAM_MAX_STAM,
    DIT_STR,
    DIT_INT,
    DIT_DEX,
    DIT_WEIGHT,
    DIT_MAX_WEIGHT,
    DIT_WEIGHT_MAX_WEIGHT,
    DIT_ARMOR,
    DIT_GOLD,
    DIT_X,
    DIT_Y,
    DIT_Z,
    DIT_OBJECT = 1000
};

class CDisplayInfo
{
public:
    CDisplayInfo() = default;
    CDisplayInfo(const astr_t &name, const astr_t &command, const DISPLAY_ITEM_TYPE &type)
        : Name(name)
        , Command(command)
        , Type(type)
    {
    }
    ~CDisplayInfo() = default;
    astr_t Name{ "" };
    astr_t Command{ "" };
    DISPLAY_ITEM_TYPE Type{ DIT_NONE };
};

class CDisplayItem
{
public:
    DISPLAY_ITEM_TYPE Type = DIT_NONE;
    astr_t Text = "";
    uint16_t Graphic = 0;
    uint16_t Hue = 0;
    uint32_t MinColor = 0;
    uint32_t MidColor = 0;
    uint32_t Color = 0;
    bool Background = false;
    uint32_t BackgroundMinColor = 0x007F0000;
    uint32_t BackgroundMidColor = 0x00FFFF00;
    uint32_t BackgroundColor = 0;
    int MinValue = 0;
    int MidValue = 0;
    bool Percents = false;

    CDisplayItem() = default;
    CDisplayItem(
        const DISPLAY_ITEM_TYPE &type,
        astr_t text,
        uint16_t graphic,
        uint16_t hue,
        uint32_t minColor,
        uint32_t midColor,
        uint32_t color,
        bool background,
        uint32_t backgroundMinColor,
        uint32_t backgroundMidColor,
        uint32_t backgroundColor,
        int minValue,
        int midValue,
        bool percents);
    CDisplayItem(const CDisplayItem &obj);
    // rule of three: the copy ctor is user-declared, so the implicit copy
    // assignment is deprecated (gcc -Wdeprecated-copy at every m_Item = ...)
    CDisplayItem &operator=(const CDisplayItem &obj);
    ~CDisplayItem() = default;
};

} // namespace xuoassist
