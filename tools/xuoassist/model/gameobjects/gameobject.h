// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (gameobjects/gameobject.h): strings are std::string,
// lists are std::vector, string lists are std::vector<std::string>.

#pragma once

#include "../basequeue.h"
#include "core/global.h"
#include "../commonitems/ignoreitem.h"

namespace xuoassist {

//! Game object class
class CGameObject : public CBaseQueueItem
{
    //! Object serial
    SETGET(uint, Serial, 0)
    //! Object graphic index
    SETGET(ushort, Graphic, 0)
    //! Object color
    SETGET(ushort, Color, 0)
    //! World X coordinate
    SETGET(short, X, 0)
    //! World Y coordinate
    SETGET(short, Y, 0)
    //! World Z coordinate
    SETGET(char, Z, 0)
    //! Serial of the container holding the object (0xFFFFFFFF - object is in the world)
    SETGET(uint, Container, 0xFFFFFFFF)
    //! Object map index
    SETGET(uchar, MapIndex, 0)
    //! Count
    SETGET(uint, Count, 0)
    //! Flags from the server
    SETGETE(uchar, Flags, 0, OnChangeFlags)
    //! Name
    SETGET(string, Name, "")
    //! NPC or item
    SETGET(bool, NPC, false)
    //! Ignore the item during searches
    SETGET(bool, Ignored, false)
    SETGET(bool, SA_Poisoned, false)

public:
    CGameObject(const uint &serial = 0);
    virtual ~CGameObject();

    void AddObject(CGameObject *obj);

    void AddItem(CGameObject *obj);

    void Reject(CGameObject *obj);

    void Clear();
    void ClearUnequipped();

    bool Frozen() { return (m_Flags & 0x01); }
    bool Poisoned();
    bool Flying();
    bool YellowHits() { return (m_Flags & 0x08); }
    bool IgnoreCharacters() { return (m_Flags & 0x10); }
    bool Locked() { return !(m_Flags & 0x20); }
    bool InWarMode() { return (m_Flags & 0x40); }
    bool Hidden() { return (m_Flags & 0x80); }

    virtual bool IsHuman() { return false; }
    virtual bool IsPlayer() { return false; }
    bool IsCorpse() { return (m_Graphic == 0x2006); }

    CGameObject *GetTopObject();

    class CGameItem *FindLayer(const int &layer);

    int CountItems(const CFindItem &findListItem, const bool &recurse = true);

    int CountItems(
        const ushort &graphic = 0xFFFF, const ushort &color = 0xFFFF, const bool &recurse = true);

    CGameObject *FindType(
        const CFindItem &findListItem,
        const vector<CIgnoreItem> &ignoreList,
        const uchar &flags,
        const bool &recurseSearch,
        vector<string> &list);
};

int GetDistance(CGameObject *obj1, CGameObject *obj2);
int GetDistance(const int &x1, const int &y1, const int &x2, const int &y2);

} // namespace xuoassist

