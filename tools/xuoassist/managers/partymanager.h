// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/partymanager.h), trimmed
// (strings are std::string).

#pragma once

#include "core/global.h"
#include "model/datareader.h"

namespace xuoassist {

struct PARTY_MEMBER_INFO
{
    uint Serial{ 0 };
    int X{ 0 };
    int Y{ 0 };
    uint Map{ 0 };
    string Name{ "" };
};

class CPartyManager
{
    // Serial of the party leader
    SETGET(uint, Leader, 0)
    // Serial of whoever invited us to the party
    SETGET(uint, Inviter, 0)
    // Whether the party can loot a player corpse
    SETGET(bool, CanLoot, false)

public:
    CPartyManager();

    // Party members
    uint Member[10];
    PARTY_MEMBER_INFO m_MemberInfo[10];

    // Party packet handling
    bool OnPacketReceived(CDataReader &reader);

    // Whether the player's party contains the given serial
    bool Contains(const uint &serial);

    void SaveMemberInfo(class CGameCharacter *obj);

    void SetMemberInfo(const PARTY_MEMBER_INFO &info);

    // Clear the party
    void Clear();
};

extern CPartyManager g_PartyManager;

} // namespace xuoassist

