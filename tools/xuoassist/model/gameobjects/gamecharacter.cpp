// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (gameobjects/gamecharacter.cpp), verbatim.

#include "gamecharacter.h"
#include "gameplayer.h"
#include "gameitem.h"

namespace xuoassist {

CGameCharacter::CGameCharacter(const uint &serial)
    : CGameObject(serial)
{
    m_NPC = true;
}

CGameCharacter::~CGameCharacter()
{
}

} // namespace xuoassist
