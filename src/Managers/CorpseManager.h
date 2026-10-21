// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2017 Hotride

#pragma once

#include <stdint.h>
#include <deque>
class CGameObject;

struct CCorpse
{
public:
    uint32_t CorpseSerial = 0;
    uint32_t ObjectSerial = 0;
    uint8_t Direction = 0;
    bool Running = false;

    CCorpse() = default;
    CCorpse(int corpseSerial, int objectSerial, uint8_t direction, bool running)
        : CorpseSerial(corpseSerial)
        , ObjectSerial(objectSerial)
        , Direction(direction)
        , Running(running)
    {
    }
    ~CCorpse() = default;
};

// A corpse waiting to be opened when the player comes within reach
struct CCorpseToOpen
{
    uint32_t Serial = 0;
    uint32_t Timer = 0; // g_Ticks stamp at which the entry expires

    CCorpseToOpen() = default;
    CCorpseToOpen(int serial, int timer)
        : Serial(serial)
        , Timer(timer)
    {
    }
    ~CCorpseToOpen() = default;
};

class CCorpseManager
{
private:
    std::deque<CCorpse> m_List;
    std::deque<CCorpseToOpen> m_AutoOpenList;

public:
    CCorpseManager() = default;
    ~CCorpseManager() = default;

    void Add(const CCorpse &corpse);
    void Remove(int corpseSerial, int objectSerial);
    bool InList(int corpseSerial, int objectSerial);
    CGameObject *GetCorpseObject(int serial);
    // Register a corpse for auto-open once the player is within useDistance;
    // a repeated serial refreshes its expiry stamp
    void AddAutoOpen(int serial, int timer);
    // Open the first registered corpse in range; useDelay is the minimum
    // interval in ms between two object uses (lastUseTime is the caller's
    // last-use stamp). Returns true when a corpse was opened.
    bool CheckCorpses(int useDistance, int useDelay, uint32_t lastUseTime);
    void Clear()
    {
        m_List.clear();
        m_AutoOpenList.clear();
    }
};

extern CCorpseManager g_CorpseManager;
