// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (assist/assist.cpp static helpers). See textconv.h.

#include "textconv.h"
#include <cstdio>
#include <cstdlib>
#include "global.h"

namespace xuoassist {

uint TextToUInt(const string &text)
{
    uint result = (uint)strtoul(text.c_str(), nullptr, 10);

    if (result == 0u)
    {
        // original: text.toInt() first, then toUInt(nullptr, 16) on failure
        const char *str = text.c_str();
        bool decimal = (text.length() != 0);
        for (size_t i = 0; i < text.length() && decimal; i++)
        {
            if (str[i] < '0' || str[i] > '9')
            {
                decimal = false;
            }
        }

        if (!decimal)
        {
            result = (uint)strtoul(str, nullptr, 16);
        }
    }

    return result;
}

uint TextToSerial(string text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }

    uint result = 0;

    if (text == "self")
    {
        result = g_PlayerSerial;
    }
    else if (text == "lasttarget")
    {
        result = g_LastTargetObject;
    }
    else if (text == "lastattack")
    {
        result = g_LastAttackObject;
    }
    else if (text == "lastcorpse")
    {
        result = g_LastCorpseObject;
    }
    else if (text == "lastcontainer")
    {
        result = g_LastContainerObject;
    }
    else if (text == "laststatus")
    {
        result = g_LastStatusObject;
    }
    else if (text == "lastobject")
    {
        result = g_LastUseObject;
    }
    else if (text == "backpack")
    {
        result = g_Backpack;
    }
    else if (text == "ground")
    {
        result = 0xFFFFFFFF;
    }
    else
    {
        // PORT-TODO(lists): g_TabListsObjects->SeekObject(text) once the
        // objects-list module is ported.
        result = 0;
    }

    if (result == 0u)
    {
        result = TextToUInt(text);
    }

    return result;
}

ushort TextToGraphic(string text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }

    // PORT-TODO(lists): g_TabListsTypes->SeekType(text) once the types-list
    // module is ported.
    ushort result = 0;

    if (result == 0u)
    {
        result = (ushort)TextToUInt(text);
    }

    return result;
}

string SerialToText(const uint &value)
{
    char buf[64] = { 0 };
    snprintf(buf, sizeof(buf), "0x%08X", value);

    return buf;
}

string GraphicToText(const ushort &value)
{
    char buf[64] = { 0 };
    snprintf(buf, sizeof(buf), "0x%04X", value);

    return buf;
}

} // namespace xuoassist
