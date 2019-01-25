// MIT License
// Copyright (C) August 2017 Hotride

#pragma once

class CCorpse
{
public:
    uint32_t CorpseSerial = 0;
    uint32_t ObjectSerial = 0;
    uint8_t Direction = 0;
    bool Running = false;

    CCorpse() {}
    CCorpse(int corpseSerial, int objectSerial, uint8_t direction, bool running)
        : CorpseSerial(corpseSerial)
        , ObjectSerial(objectSerial)
        , Direction(direction)
        , Running(running)
    {
    }
    ~CCorpse() {}
};

class CCorpseManager
{
private:
    deque<CCorpse> m_List;

public:
    CCorpseManager() {}
    ~CCorpseManager() {}

    void Add(const CCorpse &corpse);
    void Remove(int corpseSerial, int objectSerial);
    bool InList(int corpseSerial, int objectSerial);
    CGameObject *GetCorpseObject(int serial);
    void Clear() { m_List.clear(); }
};

extern CCorpseManager g_CorpseManager;
