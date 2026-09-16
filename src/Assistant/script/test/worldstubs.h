// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Stand-in definitions for the client's world objects: the script harness
// links the assistant executor without the client, and the executor reads the
// client's world model through core/world.h. This header declares the same
// global-namespace names with just the surface the linked translation units
// touch; everything here stays inert (g_World/g_Player are null, matching a
// disconnected client). The real definitions live in src/GameObjects/.

#pragma once

#include <stdint.h>
#include <xuocore/enumlist.h>
#include "BaseQueue.h"
#include "../../core/global.h"

class CGameItem;

class CGameObject : public CBaseQueueItem
{
public:
    uint32_t Serial = 0;
    uint16_t Graphic = 0;
    uint16_t Color = 0;
    uint32_t Container = 0xFFFFFFFF;
    uint8_t MapIndex = 0;
    uint32_t Count = 0;
    uint8_t m_Flags = 0;
    bool NPC = false;
    bool Ignored = false;
    astr_t m_Name;

    int m_X = 0;
    int m_Y = 0;
    char m_Z = 0;

    astr_t GetName() { return m_Name; }
    void SetName(const astr_t &name) { m_Name = name; }
    int GetX() { return m_X; }
    int GetY() { return m_Y; }
    char GetZ() { return m_Z; }
    uint8_t GetFlags() const { return m_Flags; }

    bool IsCorpse() const { return Graphic == 0x2006; }
    bool IsHuman() const { return false; }
    bool IsPlayer() { return false; }
    bool IsParalyzed() const { return false; }
    bool IsPoisoned() const { return false; }
    bool IsFlying() const { return false; }
    bool YellowHits() { return false; }
    bool IgnoreCharacters() { return false; }
    bool Locked() { return false; }
    bool InWarMode() { return false; }
    bool Hidden() { return false; }

    virtual CGameItem *FindLayer(int) const { return nullptr; }
};

class CGameItem : public CGameObject
{
public:
    uint8_t Layer = 0;
};

class CGameCharacter : public CGameObject
{
public:
    uint16_t Hits = 0;
    uint16_t MaxHits = 0;
    uint16_t Mana = 0;
    uint16_t MaxMana = 0;
    uint16_t Stam = 0;
    uint16_t MaxStam = 0;
    GENDER Gender = GENDER_MALE;
    RACE_TYPE Race = RT_HUMAN;
    uint8_t Direction = 0;
    uint8_t Notoriety = 0;
    bool CanChangeName = false;
    bool ProfileReceived = false;
    astr_t Profile;
    astr_t Title;
};

class CPlayer : public CGameCharacter
{
public:
    short Str = 0;
    short Int = 0;
    short Dex = 0;
    uint8_t LockStr = 0;
    uint8_t LockInt = 0;
    uint8_t LockDex = 0;
    uint16_t MaxWeight = 0;
    uint16_t Weight = 0;
    short Armor = 0;
    uint32_t Gold = 0;
    uint16_t StatsCap = 0;
    uint8_t Followers = 0;
    uint8_t MaxFollowers = 5;
    short FireResistance = 0;
    short ColdResistance = 0;
    short PoisonResistance = 0;
    short EnergyResistance = 0;
    short Luck = 0;
    short MinDamage = 0;
    short MaxDamage = 0;
    uint32_t TithingPoints = 0;
    int StealthSteps = 0;

    bool IsPlayer() { return true; }
    virtual CGameItem *FindBandage() { return nullptr; }
};

class CGameWorld
{
public:
    CGameObject *m_Items = nullptr;

    virtual CGameObject *FindWorldObject(uint32_t);
    CGameItem *FindWorldItem(uint32_t);
    CGameCharacter *FindWorldCharacter(uint32_t);
};

// the client skill table (Managers/SkillsManager.h in the real client)
struct CSkill
{
    bool Button = false;
    astr_t Name;
    float BaseValue = 0.0f;
    float Value = 0.0f;
    float OldValue = 0.0f;
    float Cap = 0.0f;
    uint8_t Status = 0;
};

class CSkillsManager
{
public:
    uint32_t Count = 0;
    CSkill *Get(uint32_t) { return nullptr; }
    int Find(const astr_t &) { return -1; }
    // mirrors the real CSkillsManager::Use (Managers/SkillsManager.cpp)
    void Use(const astr_t &name);
};

extern CGameWorld *g_World;
extern CPlayer *g_Player;
extern CSkillsManager g_SkillsManager;

// the client send bridge (CrossUO.h in the real client)
class CGame
{
public:
    void UseSkill(int);
    // the config accessors the executor calls; inert while disconnected
    int ValueInt(const VALUE_KEY_INT &, int);
};

extern CGame g_Game;

// the stub Find always misses, so the found path (the CGame::UseSkill send,
// inert here) is unreachable and the not-found print (inside the real Use)
// never fires through this stand-in either
inline void CSkillsManager::Use(const astr_t &name)
{
    const int index = Find(name);
    if (index != -1)
    {
        g_Game.UseSkill(index);
    }
}

namespace xuoassist {

// worldutil.h declarations the executor calls; the harness stubs the bodies
int EquipLayer(const CGameItem *item);
int Distance(int x1, int y1, int x2, int y2);

} // namespace xuoassist
