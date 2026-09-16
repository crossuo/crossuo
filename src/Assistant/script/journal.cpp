// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See journal.h. The Find loop is the CJournalManager::FindLine port
// (managers/journalmanager.cpp) with the m_List deque swapped for a
// capped vector ring; Ignore/ignorefriends flag parsing stays in the caller.

#include "journal.h"

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include <algorithm>

namespace xuoassist {

namespace {

// The original app capped the journal at 200 lines (journalmanager.cpp ctor).
constexpr size_t JOURNAL_CAP = 200;

vector<CJournalMessage> s_Messages;
bool s_IgnoreCase = false;

bool contains(const astr_t &text, const astr_t &pattern)
{
    return text.find(pattern) != astr_t::npos;
}

} // namespace

void ScriptJournal_Add(const CJournalMessage &message)
{
    s_Messages.push_back(message);
    if (s_Messages.size() > JOURNAL_CAP)
    {
        s_Messages.erase(s_Messages.begin());
    }
}

void ScriptJournal_Clear()
{
    s_Messages.clear();
}

void ScriptJournal_SetIgnoreCase(bool value)
{
    s_IgnoreCase = value;
}

bool ScriptJournal_IgnoreCase()
{
    return s_IgnoreCase;
}

CJournalMessage *ScriptJournal_Find(
    const astr_t &pattern,
    uint serial,
    ushort color,
    uchar type,
    uint startTime,
    uint endTime,
    bool remove)
{
    if (pattern.empty() || s_Messages.empty())
    {
        return nullptr;
    }

    // pattern split (was pattern.split("|") + the findID index per alternative)
    vector<astr_t> patterns;
    {
        astr_t current;
        for (char c : pattern)
        {
            if (c == '|')
            {
                patterns.push_back(current);
                current.clear();
            }
            else
            {
                current += c;
            }
        }
        patterns.push_back(current);
    }

    const bool ignoreCase = s_IgnoreCase;
    astr_t lowered;
    if (ignoreCase)
    {
        lowered = str_lower(pattern);
        // re-split lowered
        patterns.clear();
        astr_t current;
        for (char c : lowered)
        {
            if (c == '|')
            {
                patterns.push_back(current);
                current.clear();
            }
            else
            {
                current += c;
            }
        }
        patterns.push_back(current);
    }

    for (int i = (int)s_Messages.size() - 1; i >= 0; i--)
    {
        CJournalMessage &message = s_Messages[i];

        if (serial != 0u && serial != message.Serial)
        {
            continue;
        }
        if (color != 0xFFFF && color != message.Color)
        {
            continue;
        }
        if (type != 0u && (type & message.Flags) == 0)
        {
            continue;
        }
        if (message.Timer < startTime)
        {
            continue;
        }
        if (endTime != 0u && message.Timer > endTime)
        {
            continue;
        }

        astr_t text = message.Text;
        if (ignoreCase)
        {
            text = str_lower(text);
        }

        int findID = 0;
        for (const astr_t &p : patterns)
        {
            if (contains(text, p))
            {
                message.FindTextID = findID;
                if (remove)
                {
                    CJournalMessage *copy = new CJournalMessage(message);
                    s_Messages.erase(s_Messages.begin() + i);
                    return copy;
                }
                return new CJournalMessage(message);
            }
            findID++;
        }
    }

    return nullptr;
}

const vector<CJournalMessage> &ScriptJournal_Messages()
{
    return s_Messages;
}

int ScriptJournal_Count()
{
    return (int)s_Messages.size();
}

// Remove-after-read for JournalLine(index, consume). The original JournalLine had
// no consume argument and never removed - this is
// the port extension's removal half. Main thread (drain) only.
bool ScriptJournal_RemoveAt(int index)
{
    if (index < 0 || index >= (int)s_Messages.size())
    {
        return false;
    }
    s_Messages.erase(s_Messages.begin() + index);
    return true;
}

} // namespace xuoassist
