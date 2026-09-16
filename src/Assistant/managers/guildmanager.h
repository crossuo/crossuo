// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/guildmanager.h), trimmed
// (strings are astr_t).

#pragma once

#include "core/global.h"
#include <unordered_map>

namespace xuoassist {

struct GUILD_MEMBER_INFO
{
    uint32_t Serial{ 0 };
    int X{ 0 };
    int Y{ 0 };
    uint32_t Map{ 0 };
    astr_t Name{ "guild member" };
};

class GuildManager
{
public:
    GuildManager() = default;

    GuildManager(const GuildManager &) = delete;
    GuildManager &operator=(const GuildManager &) = delete;

    void SetMemberName(uint32_t serial, const astr_t &name);

    void SetMemberInfo(const GUILD_MEMBER_INFO &info);

    vector<GUILD_MEMBER_INFO> GetMembers() const;

    // the live store for the map window overlay pass (GetMembers() copies;
    // the draw path must not allocate per frame)
    const std::unordered_map<uint32_t, GUILD_MEMBER_INFO> &GetMembersMap() const
    {
        return m_GuildMembers;
    }

    bool Contains(const uint32_t &serial) const;

private:
    std::unordered_map<uint32_t, GUILD_MEMBER_INFO> m_GuildMembers;
};

extern GuildManager g_GuildManager;

} // namespace xuoassist
