// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See scriptstate.h. The Qt sources are managers/commandmanager.cpp
// (OnCommandSetGlobal/GetGlobal/ClearGlobals 3973-4000, the Timer family
// 4462-4519 over m_Timers/m_Time) and OnCommandBuffExists (3602, over
// g_BuffManager).

#include "scriptstate.h"

#include <algorithm>
#include <map>

namespace xuoassist {

namespace {

std::map<string, string> s_Globals;
std::map<string, uint> s_Timers; // stored stamp: g_Ticks - value
// key -> icon graphic: the lowered buff name, plus its "0x%04X" graphic text
// (so scripts can ask by either form and the remove event - graphic-only -
// can erase every key of the icon)
std::map<string, ushort> s_Buffs;

string to_lower_copy(string text)
{
    std::transform(text.begin(), text.end(), text.begin(), ::tolower);
    return text;
}

// The Qt BuffExists body trimmed + lowered the script argument (the port
// lowers at store time, so only the lookup needs it here).
string buff_key(string name)
{
    // leading/trailing space trim (QString::trimmed parity)
    size_t begin = name.find_first_not_of(" \t\r\n");
    if (begin == string::npos)
    {
        return "";
    }
    size_t end = name.find_last_not_of(" \t\r\n");
    return to_lower_copy(name.substr(begin, end - begin + 1));
}

} // namespace

// --- globals ---------------------------------------------------------------

void ScriptGlobal_Set(const string &name, const string &value)
{
    if (name.empty())
    {
        return;
    }
    s_Globals[name] = value;
}

string ScriptGlobal_Get(const string &name)
{
    std::map<string, string>::const_iterator it = s_Globals.find(name);
    return it != s_Globals.end() ? it->second : string();
}

void ScriptGlobal_Clear()
{
    s_Globals.clear();
}

// --- timers ----------------------------------------------------------------

void ScriptTimer_Set(const string &name, int value)
{
    s_Timers[name] = g_Ticks - (uint)value; // Qt: m_Time.elapsed() - value
}

int ScriptTimer_Get(const string &name)
{
    std::map<string, uint>::const_iterator it = s_Timers.find(name);
    return it != s_Timers.end() ? (int)(g_Ticks - it->second) : -1;
}

bool ScriptTimer_Exists(const string &name)
{
    return s_Timers.find(name) != s_Timers.end();
}

void ScriptTimer_Remove(const string &name)
{
    s_Timers.erase(name);
}

void ScriptTimer_Clear()
{
    s_Timers.clear();
}

// --- buffs -----------------------------------------------------------------

void ScriptBuffs_Add(ushort graphic, uint timer, const string &name)
{
    (void)timer;
    const string key = buff_key(name);
    if (!key.empty())
    {
        s_Buffs[key] = graphic;
    }
    char graphicText[16] = { 0 };
    snprintf(graphicText, sizeof(graphicText), "0x%04X", graphic);
    s_Buffs[graphicText] = graphic;
}

void ScriptBuffs_Remove(ushort graphic)
{
    // the remove event carries only the icon id: erase its graphic key plus
    // every name key mapped to it
    char graphicText[16] = { 0 };
    snprintf(graphicText, sizeof(graphicText), "0x%04X", graphic);
    s_Buffs.erase(graphicText);
    for (std::map<string, ushort>::iterator it = s_Buffs.begin(); it != s_Buffs.end();)
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

bool ScriptBuffs_Exists(const string &name)
{
    const string key = buff_key(name);
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
    snprintf(graphicText, sizeof(graphicText), "0x%04X", (ushort)strtoul(key.c_str(), nullptr, 16));
    return s_Buffs.find(graphicText) != s_Buffs.end();
}

} // namespace xuoassist
