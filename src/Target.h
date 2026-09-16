// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <stdint.h>

#if defined(XUO_ASSISTANT)
#include <common/str.h> // astr_t (the client string typedef)
#include <vector>
#endif

class CMulti;
class CMultiObject;
struct CDataReader;

#if defined(XUO_ASSISTANT)
// The WaitTarget hook queue entry (was xuoassist::CTargetHook).
class CTargetHook
{
public:
    CTargetHook() {}
    explicit CTargetHook(const uint32_t &serial);
    CTargetHook(const astr_t &tile, const int &x, const int &y, const int &z, const bool &relative);
    ~CTargetHook() {}

    bool TargetOnObject{ true };
    uint32_t Serial{ 0 };
    astr_t Tile{ "" };
    int X{ 0 };
    int Y{ 0 };
    int Z{ 0 };
    bool Relative{ false };
};
#endif

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

#if defined(XUO_ASSISTANT)
    // a cursor the assistant itself requested (CAssistant::RequestTarget):
    // the next reply is consumed and saved for the armed-handler replay
    bool FromAssist = false;
    // a server cursor reply was seen (consumed or not)
    bool ClientWantTarget = false;
    // the tile the last tile-target answered with (`infotile lasttile`)
    uint16_t LastTileGraphic = 0;
    uint16_t LastTileX = 0;
    uint16_t LastTileY = 0;
    uint16_t LastTileZ = 0;
#endif

private:
    uint8_t m_Data[19];
    uint8_t m_LastData[19];
    CMulti *m_Multi{ nullptr };

#if defined(XUO_ASSISTANT)
    // the open-cursor mirror the WaitTarget-hook senders gate on; client
    // Targeting stays false whenever a reply is consumed
    bool m_HookTargeting{ false };
    std::vector<CTargetHook> m_WaitTargets;
    std::vector<uint8_t> m_CurrentClientTarget;
#endif

    void AddMultiObject(CMultiObject *obj);

public:
    CTarget();
    ~CTarget() {}

    void SetLastTargetObject(int serial);
    // Parses a cursor reply (0x6C / 0x99). Returns false when the reply was
    // consumed (WaitTarget hook, assistant UI selection or a playing macro):
    // the client keeps no open cursor and skips its own processing.
    bool SetData(CDataReader &reader);
    bool SetMultiData(CDataReader &reader);
    // The assistant-requested cursor (CAssistant::RequestTarget) and the
    // cursor close (the script/macro/hotkey cancels): the former injected
    // 0x6C packets, now set on the client target directly.
    void RequestCursor();
    void CancelCursor();
    // The saved-server-cursor replay: the assistant consumed a real 0x6C/0x99
    // reply on the WaitTarget path; this runs the Target/MultiPlacement
    // handler bodies on the raw bytes.
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

#if defined(XUO_ASSISTANT)
    // The WaitTarget hook queue (set-/wait-target): CheckHook answers an
    // incoming cursor from the queue front.
    bool CheckHook();
    void SetObjectsHook(const std::vector<astr_t> &objects);
    void SetTileHook(
        const astr_t &tile, const int &x, const int &y, const int &z, const bool &relative);
    void ClearHooks();
    // The replay of a reply a pending assistant UI selection consumed.
    void RestoreTarget();
    bool HookTargeting() const { return m_HookTargeting; }
    // The consume-path senders: raw 0x6C builders for answers to a cursor
    // the client never saw (no recording, no LastTarget bookkeeping, no
    // route through SendTarget).
    void SendHookTargetObject(const uint32_t &serial);
    void SendHookTargetTile(const uint16_t tileID, const short &x, const short &y, short z);
    void SendHookTargetLastTile();
    void SendHookCancelTarget();
#endif
};

extern CTarget g_Target;
