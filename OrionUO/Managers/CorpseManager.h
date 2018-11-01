/***********************************************************************************
**
** CorpseManager.h
**
** Copyright (C) August 2017 Hotride
**
************************************************************************************
*/

#ifndef CORPSEMANAGER_H
#define CORPSEMANAGER_H

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

//!Класс менеджера трупов
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

//!Менеджер трупов
extern CCorpseManager g_CorpseManager;

#endif //CORPSEMANAGER_H
