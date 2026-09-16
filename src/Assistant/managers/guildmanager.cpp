// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/guildmanager.cpp).

#include "guildmanager.h"
#include "../core/assistantevents.h"

namespace xuoassist {

GuildManager g_GuildManager;

GuildManager::GuildManager() {}

void GuildManager::SetMemberInfo(const GUILD_MEMBER_INFO &info)
{
    std::unordered_map<uint32_t, GUILD_MEMBER_INFO>::iterator found = m_GuildMembers.find(info.Serial);

    if (found != m_GuildMembers.end())
    {
        found->second.X = info.X;
        found->second.Y = info.Y;
        found->second.Map = info.Map;
    }
    else
    {
        m_GuildMembers[info.Serial] = info;
    }

    // Was g_Map->CheckForUpdate(info.Serial) when the map window is open.
    g_AssistantEvents->OnMapUpdate(info.Serial);
}

void GuildManager::SetMemberName(uint32_t serial, astr_t &name)
{
    std::unordered_map<uint32_t, GUILD_MEMBER_INFO>::iterator found = m_GuildMembers.find(serial);

    if (found != m_GuildMembers.end())
    {
        found->second.Name = name;
    }
    else
    {
        GUILD_MEMBER_INFO info;
        info.Name = name;
        m_GuildMembers[serial] = info;
    }
}

vector<GUILD_MEMBER_INFO> GuildManager::GetMembers()
{
    vector<GUILD_MEMBER_INFO> members;

    for (std::unordered_map<uint32_t, GUILD_MEMBER_INFO>::iterator i = m_GuildMembers.begin();
         i != m_GuildMembers.end();
         ++i)
    {
        members.push_back(i->second);
    }

    return members;
}

bool GuildManager::Contains(const uint32_t &serial)
{
    std::unordered_map<uint32_t, GUILD_MEMBER_INFO>::iterator found = m_GuildMembers.find(serial);
    return found != m_GuildMembers.end();
}

} // namespace xuoassist
