// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CMulti;
class CMultiObject;

class CTarget
{
public:
    uint8_t Type = 0;
    uint8_t CursorType = 0;
    uint16_t MultiGraphic = 0;
    uint16_t MultiX = 0;
    uint16_t MultiY = 0;
    uint32_t CursorID = 0;
    bool Targeting = false;

private:
    uint8_t m_Data[19];
    uint8_t m_LastData[19];
    CMulti *m_Multi{ nullptr };

    void AddMultiObject(CMultiObject *obj);

public:
    CTarget();
    ~CTarget() {}

    void SetLastTargetObject(int serial);
    void SetData(Wisp::CDataReader &reader);
    void SetMultiData(Wisp::CDataReader &reader);
    bool IsTargeting() const { return Targeting; }
    void Reset();
    void RequestFromCustomHouse();
    void SendTargetObject(int Serial);
    void SendTargetTile(uint16_t tileID, short x, short Y, char z);
    void SendCancelTarget();
    void Plugin_SendTargetObject(int Serial);
    void Plugin_SendTargetTile(uint16_t tileID, short x, short Y, char z);
    void Plugin_SendCancelTarget();
    void Plugin_SendTarget();
    void SendLastTarget();
    void SendTarget();
    void LoadMulti(int offsetX, int offsetY, char offsetZ);
    void UnloadMulti();
    CMulti *GetMultiAtXY(short x, short y);
};

extern CTarget g_Target;
