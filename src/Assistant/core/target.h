// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (assist/target.h), trimmed.
//
// Split decision (master plan §4.1 "assist/target.* PORT WITH CHANGES" and
// §4.4 target.cpp row): the client already has its own CTarget (src/Target.h,
// global ::g_Target) that owns the client-facing target state - cursor data
// buffers, multi placement, custom-house requests and the client-side send
// paths. That class is untouched and stays authoritative for the client.
//
// This class (namespace xuoassist, same original name CTarget as the rest of
// the assistant port) keeps only the ASSISTANT-OWNED parts of the
// original assist/target.cpp, which ran out-of-process and had to duplicate the
// client's cursor bookkeeping to work:
//   - the WaitTarget hook queue (CTargetHook, CheckHook, SetObjectsHook /
//     SetTileHook / ClearHooks) - pure assistant feature;
//   - SetSendData (assistant-side interception of outgoing 0x6C: macro
//     recording + LastTarget tracking) and SetRecvAssistData (the target
//     cursor the assistant itself requested via CPacketToClientTargetRequest);
//   - SetRecvData/SetMultiData bookkeeping for assistant-triggered replies
//     (FromAssist/ClientWantTarget + the saved client packet for
//     RestoreTarget) and LastTile tracking for the `infotile lasttile`
//     command;
//   - the assistant target sends (SendTargetObject/SendTargetTile/...), which
//     go out as real packets exactly like the original app's did.
// The duplicated cursor fields (Type/CursorID/CursorType/Targeting/
// MultiGraphic) are required by the code above; the file-manager tile
// resolution used by CheckHook lands with the filemanager dedup round.

#pragma once

#include "global.h"
#include "../model/datareader.h"

namespace xuoassist {

class CTargetHook
{
public:
    CTargetHook() {}
    explicit CTargetHook(const uint &serial);
    CTargetHook(const string &tile, const int &x, const int &y, const int &z, const bool &relative);
    ~CTargetHook() {}

    bool TargetOnObject{ true };
    uint Serial{ 0 };
    string Tile{ "" };
    int X{ 0 };
    int Y{ 0 };
    int Z{ 0 };
    bool Relative{ false };
};

class CTarget
{
    SETGET(uchar, Type, 0)
    SETGET(uchar, CursorType, 0)
    SETGET(ushort, MultiGraphic, 0)
    SETGET(bool, FromAssist, false)
    SETGET(bool, ClientWantTarget, false)

    SETGET(ushort, LastTileGraphic, 0)
    SETGET(ushort, LastTileX, 0)
    SETGET(ushort, LastTileY, 0)
    SETGET(ushort, LastTileZ, 0)

private:
    uint m_CursorID{ 0 };
    bool m_Targeting{ false };
    vector<uchar> m_CurrentClientTarget;
    vector<CTargetHook> m_WaitTargets;

public:
    CTarget();
    ~CTarget() {}

    bool CheckHook();
    void SetObjectsHook(const vector<string> &objects);
    void SetTileHook(const string &tile, const int &x, const int &y, const int &z, const bool &relative);
    void ClearHooks();
    void RestoreTarget();
    bool SetSendData(CDataReader &reader);
    void SetRecvAssistData(CDataReader &reader);
    bool SetRecvData(CDataReader &reader);
    bool SetMultiData(CDataReader &reader);
    bool IsTargeting() const { return m_Targeting; }
    void DumpLastTileInfo();
    void Reset();
    void SendTargetObject(const uint &Serial);
    void SendTargetTile(const ushort &tileID, const short &x, const short &y, short z);
    void SendTargetLastTile();
    void SendCancelTarget();
    void SendTargetLastTarget();
};

extern CTarget g_Target;

} // namespace xuoassist

