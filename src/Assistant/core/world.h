// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The client's world objects (src/GameObjects/) are the single world model:
// the client g_World/g_Player singletons are read (and the assistant's own
// bookkeeping kept on the same objects) everywhere the assistant touches
// world state. The script harness links the assistant executor without the
// client and swaps in stand-in declarations for these same names
// (script/test/worldstubs.h).

#pragma once

#if defined(XUO_ASSISTANT_HARNESS)

#include "../script/test/worldstubs.h"

#else

#include "GameObjects/GameWorld.h" // ::CGameObject/CGameItem/CGameCharacter/CGameWorld, ::g_World
#include "GameObjects/GamePlayer.h" // ::CPlayer, ::g_Player
#include "Managers/SkillsManager.h" // ::CSkill/CSkillsManager, ::g_SkillsManager

#endif
