// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (gameobjects/gameworld.h): WORLD_MAP unchanged
// (std::map), the serial-graph semantics are kept exactly.

#pragma once

#include "gameobject.h"
#include "gameitem.h"
#include "gamecharacter.h"
#include <map>

namespace xuoassist {

typedef std::map<uint, CGameObject *> WORLD_MAP;

//! Game world class
class CGameWorld
{
private:
    /*!
	Create the player
	@param [__in] serial Player serial
	@return
	*/
    void CreatePlayer(const uint &serial);

    /*!
	Remove the player
	@return
	*/
    void RemovePlayer();

public:
    CGameWorld(const uint &serial);
    ~CGameWorld();

    //! Items in memory
    WORLD_MAP m_Map;

    //! Items in the world
    CGameObject *m_Items;

    void IgnoreReset();

    void RemoveRangedObjects();

    /*!
	Make the character with the given serial the current player
	@param [__in] serial Serial of the new player
	@return
	*/
    void SetPlayer(const uint &serial);

    /*!
	Create (or fetch, if it already exists) a game item
	@param [__in] serial Item serial
	@return Pointer to the item
	*/
    CGameItem *GetWorldItem(const uint &serial);

    /*!
	Create (or fetch, if it already exists) a game character
	@param [__in] serial Character serial
	@return Pointer to the character
	*/
    CGameCharacter *GetWorldCharacter(const uint &serial);

    /*!
	Find a game object in memory
	@param [__in] serial Object serial
	@return Pointer to the object or nullptr
	*/
    CGameObject *FindWorldObject(const uint &serial);

    /*!
	Find a game item in memory
	@param [__in] serial Item serial
	@return Pointer to the item or nullptr
	*/
    CGameItem *FindWorldItem(const uint &serial);

    /*!
	Find a game character in memory
	@param [__in] serial Character serial
	@return Pointer to the character or nullptr
	*/
    CGameCharacter *FindWorldCharacter(const uint &serial);

    /*!
	Remove an object from memory
	@param [__in] obj Pointer to the object
	@return
	*/
    void RemoveObject(CGameObject *obj);

    /*!
	Take an object out of its container
	@param [__in] obj Pointer to the object
	@return
	*/
    void RemoveFromContainer(CGameObject *obj);

    /*!
	Empty the given container
	@param [__in] obj Pointer to the object (container)
	@return
	*/
    void ClearContainer(CGameObject *obj);

    /*!
	Put an object into a container
	@param [__in] obj Pointer to the object
	@param [__in] containerSerial Container serial
	@return
	*/
    void PutContainer(CGameObject *obj, const uint &containerSerial)
    {
        CGameObject *cnt = FindWorldObject(containerSerial);
        if (cnt != nullptr)
            PutContainer(obj, cnt);
    }

    /*!
	Put an object into a container
	@param [__in] obj Pointer to the object
	@param [__in] container Pointer to the container
	@return
	*/
    void PutContainer(CGameObject *obj, CGameObject *container);

    /*!
	Equip an item
	@param [__in] obj Pointer to the item
	@param [__in] containerSerial Container serial
	@param [__in] layer Layer to equip the item in
	@return
	*/
    void PutEquipment(CGameItem *obj, const uint &containerSerial, const int &layer)
    {
        CGameObject *cnt = FindWorldObject(containerSerial);
        if (cnt != nullptr)
            PutEquipment(obj, cnt, layer);
    }

    /*!
	Equip an item
	@param [__in] obj Pointer to the object
	@param [__in] container Pointer to the container
	@param [__in] layer Layer to equip the item in
	@return
	*/
    void PutEquipment(CGameItem *obj, CGameObject *container, const int &layer)
    {
        PutContainer(obj, container);
        obj->SetLayer(layer);
    }

    /*!
	Move an object to the front of the queue
	@param [__in] obj Pointer to the object
	@return
	*/
    void MoveToTop(CGameObject *obj);

    void UpdateContainedItem(
        const uint &serial,
        const ushort &graphic,
        const uchar &graphicIncrement,
        const ushort &count,
        const int &x,
        const int &y,
        const uint &containerSerial,
        const ushort &color);

    void UpdateGameObject(
        uint serial,
        ushort graphic,
        uchar graphicIncrement,
        int count,
        int x,
        int y,
        char z,
        uchar direction,
        ushort color,
        uchar flags,
        int a11,
        UPDATE_GAME_OBJECT_TYPE updateType,
        ushort a13);

    void UpdatePlayer(
        const uint &serial,
        const ushort &graphic,
        const uchar &graphicIncrement,
        const ushort &color,
        const uchar &flags,
        const int &x,
        const int &y,
        const ushort &serverID,
        const uchar &direction,
        const char &z);

    ushort FixColor(const ushort &color, const ushort &defaultColor);

    /*!
	Dump the items kept in memory
	@param [__in] nCount Indentation count
	@param [__in_opt] serial Parent serial
	@return
	*/
    void Dump(uchar nCount = 0, uint serial = 0xFFFFFFFF);
};

//! Pointer to the world
extern CGameWorld *g_World;

} // namespace xuoassist

