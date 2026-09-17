// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See scriptstate.h. The original sources are managers/commandmanager.cpp
// (OnCommandSetGlobal/GetGlobal/ClearGlobals, the Timer family
// over m_Timers/m_Time) and OnCommandBuffExists (over
// g_BuffManager).

#include "scriptstate.h"

#include <algorithm>
#include <map>

namespace xuoassist {

namespace {

std::map<astr_t, astr_t> s_Globals;
std::map<astr_t, uint32_t> s_Timers; // stored stamp: g_Ticks - value
// key -> icon graphic: the lowered buff name, plus its "0x%04X" graphic text
// (so scripts can ask by either form and the remove event - graphic-only -
// can erase every key of the icon)
std::map<astr_t, uint16_t> s_Buffs;

// The original BuffExists body trimmed + lowered the script argument (the port
// lowers at store time, so only the lookup needs it here).
astr_t buff_key(astr_t name)
{
    // leading/trailing space trim
    size_t begin = name.find_first_not_of(" \t\r\n");
    if (begin == astr_t::npos)
    {
        return "";
    }
    size_t end = name.find_last_not_of(" \t\r\n");
    return str_lower(name.substr(begin, end - begin + 1));
}

} // namespace

// --- globals ---------------------------------------------------------------

void ScriptGlobal_Set(const astr_t &name, const astr_t &value)
{
    if (name.empty())
    {
        return;
    }
    s_Globals[name] = value;
}

astr_t ScriptGlobal_Get(const astr_t &name)
{
    std::map<astr_t, astr_t>::const_iterator it = s_Globals.find(name);
    return it != s_Globals.end() ? it->second : astr_t();
}

void ScriptGlobal_Clear()
{
    s_Globals.clear();
}

// --- timers ----------------------------------------------------------------

void ScriptTimer_Set(const astr_t &name, int value)
{
    s_Timers[name] = g_Ticks - (uint32_t)value;
}

int ScriptTimer_Get(const astr_t &name)
{
    std::map<astr_t, uint32_t>::const_iterator it = s_Timers.find(name);
    return it != s_Timers.end() ? (int)(g_Ticks - it->second) : -1;
}

bool ScriptTimer_Exists(const astr_t &name)
{
    return s_Timers.find(name) != s_Timers.end();
}

void ScriptTimer_Remove(const astr_t &name)
{
    s_Timers.erase(name);
}

void ScriptTimer_Clear()
{
    s_Timers.clear();
}

// --- buffs -----------------------------------------------------------------

void ScriptBuffs_Add(uint16_t graphic, uint32_t timer, const astr_t &name)
{
    (void)timer;
    const astr_t key = buff_key(name);
    if (!key.empty())
    {
        s_Buffs[key] = graphic;
    }
    char graphicText[16] = { 0 };
    snprintf(graphicText, sizeof(graphicText), "0x%04X", graphic);
    s_Buffs[graphicText] = graphic;
}

void ScriptBuffs_Remove(uint16_t graphic)
{
    // the remove event carries only the icon id: erase its graphic key plus
    // every name key mapped to it
    char graphicText[16] = { 0 };
    snprintf(graphicText, sizeof(graphicText), "0x%04X", graphic);
    s_Buffs.erase(graphicText);
    for (std::map<astr_t, uint16_t>::iterator it = s_Buffs.begin(); it != s_Buffs.end();)
    {
        if (it->second == graphic && it->first != graphicText)
        {
            s_Buffs.erase(it++);
        }
        else
        {
            ++it;
        }
    }
}

bool ScriptBuffs_Exists(const astr_t &name)
{
    const astr_t key = buff_key(name);
    if (key.empty())
    {
        return false;
    }
    if (s_Buffs.find(key) != s_Buffs.end())
    {
        return true;
    }
    // the script may pass the icon id as text ("0x754C")
    char graphicText[16] = { 0 };
    snprintf(graphicText, sizeof(graphicText), "0x%04X", (uint16_t)strtoul(key.c_str(), nullptr, 16));
    return s_Buffs.find(graphicText) != s_Buffs.end();
}

} // namespace xuoassist
