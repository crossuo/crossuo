// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <stdint.h>

class CMulti;
class CMultiObject;
struct CDataReader;

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
    void SetData(CDataReader &reader);
    void SetMultiData(CDataReader &reader);
    // The assistant-requested cursor (CAssistant::RequestTarget) and the
    // cursor close (the script/macro/hotkey cancels): the former injected
    // 0x6C packets, now set on the client target directly.
    void RequestCursor();
    void CancelCursor();
    // The saved-server-cursor replay (xuoassist::CTarget::RestoreTarget): the
    // assistant consumed a real 0x6C/0x99 reply on the WaitTarget path; this
    // runs the client Target/MultiPlacement handler bodies on the raw bytes.
    void RestoreCursor(uint8_t *data, int size);
    bool IsTargeting() const { return Targeting; }
    void Reset();
    void RequestFromCustomHouse();
    void SendTargetObject(int Serial);
    void SendTargetTile(uint16_t tileID, short x, short Y, char z);
    void SendCancelTarget();
    void SendLastTarget();
    void SendTarget();
    void LoadMulti(int offsetX, int offsetY, char offsetZ);
    void UnloadMulti();
    CMulti *GetMultiAtXY(short x, short y);
};

extern CTarget g_Target;
