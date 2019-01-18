// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GameItem.h"

typedef map<uint32_t, CGameObject *> WORLD_MAP;

class CGameWorld
{
public:
    uint32_t ObjectToRemove = 0;

private:
    void CreatePlayer(int serial);
    void RemovePlayer();

public:
    CGameWorld(int serial);
    ~CGameWorld();

    WORLD_MAP m_Map;
    CGameObject *m_Items{ nullptr };

    void ResetObjectHandlesState();
    void ProcessAnimation();
    void ProcessSound(int ticks, CGameCharacter *gc);
    void SetPlayer(int serial);
    CGameItem *GetWorldItem(int serial);
    CGameCharacter *GetWorldCharacter(int serial);
    CGameObject *FindWorldObject(int serial);
    CGameItem *FindWorldItem(int serial);
    CGameCharacter *FindWorldCharacter(int serial);
    void ReplaceObject(CGameObject *obj, int newSerial);
    void RemoveObject(CGameObject *obj);
    void RemoveFromContainer(CGameObject *obj);
    void ClearContainer(CGameObject *obj);

    void PutContainer(CGameObject *obj, int containerSerial)
    {
        CGameObject *cnt = FindWorldObject(containerSerial);
        if (cnt != nullptr)
            PutContainer(obj, cnt);
    }

    void PutContainer(CGameObject *obj, CGameObject *container);

    void PutEquipment(CGameItem *obj, int containerSerial, int layer)
    {
        CGameObject *cnt = FindWorldObject(containerSerial);
        if (cnt != nullptr)
            PutEquipment(obj, cnt, layer);
    }

    void PutEquipment(CGameItem *obj, CGameObject *container, int layer)
    {
        PutContainer(obj, container);
        obj->Layer = layer;
    }

    void MoveToTop(CGameObject *obj);
    void Dump(uint8_t tCount = 0, uint32_t serial = 0xFFFFFFFF);
    CGameObject *SearchWorldObject(
        int serialStart, int scanDistance, SCAN_TYPE_OBJECT scanType, SCAN_MODE_OBJECT scanMode);

    void UpdateContainedItem(
        int serial,
        uint16_t graphic,
        uint8_t graphicIncrement,
        uint16_t count,
        int x,
        int y,
        int containerSerial,
        uint16_t color);

    void UpdateItemInContainer(CGameObject *obj, CGameObject *container, int x, int y);

    void UpdateGameObject(
        int serial,
        uint16_t graphic,
        uint8_t graphicIncrement,
        int count,
        int x,
        int y,
        char z,
        uint8_t direction,
        uint16_t color,
        uint8_t flags,
        int a11,
        UPDATE_GAME_OBJECT_TYPE updateType,
        uint16_t a13);

    void UpdatePlayer(
        int serial,
        uint16_t graphic,
        uint8_t graphicIncrement,
        uint16_t color,
        uint8_t flags,
        int x,
        int y,
        uint16_t serverID,
        uint8_t direction,
        char z);
};

extern CGameWorld *g_World;
