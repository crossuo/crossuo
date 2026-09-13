// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/displayitem.h): strings are std::string.

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
    CDisplayInfo() {}
    CDisplayInfo(const string &name, const string &command, const DISPLAY_ITEM_TYPE &type)
        : Name(name)
        , Command(command)
        , Type(type)
    {
    }
    ~CDisplayInfo() {}
    string Name{ "" };
    string Command{ "" };
    DISPLAY_ITEM_TYPE Type{ DIT_NONE };
};

class CDisplayItem
{
    SETGET(DISPLAY_ITEM_TYPE, Type, DIT_NONE)
    SETGET(string, Text, "")
    SETGET(ushort, Graphic, 0)
    SETGET(ushort, Hue, 0)
    SETGET(uint, MinColor, 0)
    SETGET(uint, MidColor, 0)
    SETGET(uint, Color, 0)
    SETGET(bool, Background, false)
    SETGET(uint, BackgroundMinColor, 0x007F0000)
    SETGET(uint, BackgroundMidColor, 0x00FFFF00)
    SETGET(uint, BackgroundColor, 0)
    SETGET(int, MinValue, 0)
    SETGET(int, MidValue, 0)
    SETGET(bool, Percents, false)

public:
    CDisplayItem() {}
    CDisplayItem(
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
        bool percents);
    CDisplayItem(const CDisplayItem &obj);
    // rule of three: the copy ctor is user-declared, so the implicit copy
    // assignment is deprecated (gcc -Wdeprecated-copy at every m_Item = ...)
    CDisplayItem &operator=(const CDisplayItem &obj);
    ~CDisplayItem() {}
};

} // namespace xuoassist

