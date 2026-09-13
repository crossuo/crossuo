// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/partymanager.h), trimmed
// (strings are std::string).

#pragma once

#include "core/global.h"
#include <xuocore/mappedfile.h> // ::CDataReader (the client data reader)

namespace xuoassist {

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

    // Party packet handling
    bool OnPacketReceived(CDataReader &reader);

    // Whether the player's party contains the given serial
    bool Contains(const uint &serial);

    // Clear the party
    void Clear();
};

extern CPartyManager g_PartyManager;

} // namespace xuoassist

