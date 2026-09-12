// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (assist/target.cpp); see target.h for the
// assistant-owned vs client ::g_Target split. The macro-recording branches
// (g_TabMacros) are Phase 3 and keep PORT-TODO markers.

#include "target.h"
#include "assist.h"
#include "packets.h"
#include "../config/assistantconfig.h"
#include "../model/gameobjects/gameworld.h"
#include "../model/gameobjects/gameplayer.h"

namespace xuoassist {

static string to_lower_copy(string text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }
    return text;
}

CTarget g_Target;

CTargetHook::CTargetHook(const uint &serial)
{
    TargetOnObject = true;
    Serial = serial;
}

CTargetHook::CTargetHook(const string &tile, const int &x, const int &y, const int &z, const bool &relative)
    : Tile(tile)
{
    TargetOnObject = false;
    X = x;
    Y = y;
    Z = z;
    Relative = relative;
}

CTarget::CTarget() {}

void CTarget::SetObjectsHook(const vector<string> &objects)
{
    m_WaitTargets.clear();

    if (!objects.empty())
    {
        for (const string &i : objects)
        {
            m_WaitTargets.push_back(CTargetHook(CAssistant::TextToSerial(i)));
        }
    }
}

void CTarget::SetTileHook(const string &tile, const int &x, const int &y, const int &z, const bool &relative)
{
    m_WaitTargets.clear();

    m_WaitTargets.push_back(CTargetHook(tile, x, y, z, relative));
}

void CTarget::ClearHooks()
{
    m_WaitTargets.clear();
}

bool CTarget::CheckHook()
{
    if (!m_WaitTargets.empty())
    {
        const CTargetHook &hook = m_WaitTargets.front();

        if (hook.TargetOnObject)
        {
            SendTargetObject(hook.Serial);
        }
        else
        {
            string tileName = to_lower_copy(hook.Tile);

            if (tileName == "lasttile")
            {
                SendTargetLastTile();
            }
            else
            {
                int x = hook.X;
                int y = hook.Y;
                int z = hook.Z;

                if (hook.Relative)
                {
                    x += g_Player->GetX();
                    y += g_Player->GetY();
                    z += g_Player->GetZ();
                }
                else if ((x == 0) && (y == 0))
                {
                    x = g_Player->GetX();
                    y = g_Player->GetY();
                    z = g_Player->GetZ();
                }

                // Was g_FileManager.GetTile(x, y, z, hook.Tile) + the
                // tiledata surface-height fixup:
                // PORT-TODO(phase-1-filemanager): tile-name resolution needs
                // the map/tiledata access that lands with the filemanager
                // dedup round; until then the numeric-graphic fallback below
                // behaves like the original's empty-tile-list path.
                ushort tileGraphic = CAssistant::TextToGraphic(hook.Tile);

                SendTargetTile(tileGraphic, x, y, z);
            }
        }

        m_WaitTargets.erase(m_WaitTargets.begin());

        return false;
    }

    return true;
}

void CTarget::Reset()
{
    // pure cleanup
    m_Type = 0;
    m_CursorType = 0;
    m_CursorID = 0;
    m_Targeting = false;
    m_MultiGraphic = 0;
}

void CTarget::RestoreTarget()
{
    if (m_FromAssist)
    {
        m_FromAssist = false;

        if (m_ClientWantTarget)
        {
            CDataReader reader(&m_CurrentClientTarget[0], m_CurrentClientTarget.size());

            uchar id = reader.ReadUInt8();

            if (id == 0x6C)
            {
                SetRecvData(reader);
            }
            else
            {
                SetMultiData(reader);
            }

            g_Assistant.SendClient(m_CurrentClientTarget);
        }
    }
}

bool CTarget::SetSendData(CDataReader &reader)
{
    m_Targeting = false;
    m_ClientWantTarget = false;

    if (reader.ReadUInt8() == 1) // tile
    {
        reader.Move(9);
        m_LastTileX = reader.ReadUInt16BE();
        m_LastTileY = reader.ReadUInt16BE();
        m_LastTileZ = reader.ReadUInt16BE();
        m_LastTileGraphic = reader.ReadUInt16BE();

        // PORT-TODO(phase-3): macro recording of MT_WAIT_FOR_TARGET_TILE
        // (CMacroWithTargetTile with the player-relative offsets).
    }
    else
    {
        reader.Move(5);
        uint serial = reader.ReadUInt32BE();

        // PORT-TODO(phase-3): macro recording of MT_WAIT_FOR_TARGET_OBJECT
        // (CMacroWithObject with MCT_SELF/MCT_LASTATTACK/MCT_LASTTARGET/
        // MCT_ENEMY/MCT_FRIEND conversions; see the original
        // CTarget::SetSendData).

        if (serial != g_PlayerSerial)
        {
            g_LastTargetObject = serial;
        }
    }

    return true;
}

void CTarget::SetRecvAssistData(CDataReader &reader)
{
    // And set the matching values (the target the assistant requested).
    m_Type = reader.ReadUInt8();
    m_CursorID = reader.ReadUInt32BE();
    m_CursorType = reader.ReadUInt8();
    m_Targeting = (m_CursorType < 3);
    m_MultiGraphic = 0;
}

bool CTarget::SetRecvData(CDataReader &reader)
{
    // Copy the buffer
    m_CurrentClientTarget.resize(reader.GetSize());
    memcpy(&m_CurrentClientTarget[0], reader.GetStart(), reader.GetSize());

    // And set the matching values
    m_Type = reader.ReadUInt8();
    m_CursorID = reader.ReadUInt32BE();
    m_CursorType = reader.ReadUInt8();
    m_Targeting = (m_CursorType < 3);
    m_MultiGraphic = 0;
    m_ClientWantTarget = true;

    bool result = (CheckHook() && !m_FromAssist);

    // PORT-TODO(phase-3): when a macro is playing and waits for
    // MT_WAIT_FOR_TARGET_OBJECT/MT_WAIT_FOR_TARGET_TILE, the original
    // consumed the target (PlayWaiting) and returned false.

    return result;
}

bool CTarget::SetMultiData(CDataReader &reader)
{
    m_CurrentClientTarget.resize(reader.GetSize());
    memcpy(&m_CurrentClientTarget[0], reader.GetStart(), reader.GetSize());

    // And set the matching values
    m_Type = reader.ReadUInt8();
    m_CursorID = reader.ReadUInt32BE();
    m_CursorType = reader.ReadUInt8();
    m_Targeting = (m_CursorType < 3);
    m_ClientWantTarget = true;

    reader.Move(10);
    m_MultiGraphic = reader.ReadUInt16BE() + 1;

    bool result = (CheckHook() && !m_FromAssist);

    // PORT-TODO(phase-3): macro PlayWaiting branch (see SetRecvData).

    return result;
}

void CTarget::DumpLastTileInfo()
{
    g_Assistant.DumpTileInfo(m_LastTileGraphic, m_LastTileX, m_LastTileY, m_LastTileZ);
}

void CTarget::SendTargetObject(const uint &serial)
{
    if (!m_Targeting)
    {
        return;
    }

    CPacketTargetObject(0, m_CursorID, m_CursorType, serial).SendServer();
    m_Targeting = false;
    m_MultiGraphic = 0;
    m_ClientWantTarget = false;
}

void CTarget::SendTargetTile(const ushort &tileID, const short &x, const short &y, short z)
{
    if (!m_Targeting)
    {
        return;
    }

    CPacketTargetTile(1, m_CursorID, m_CursorType, tileID, x, y, z).SendServer();
    m_Targeting = false;
    m_MultiGraphic = 0;
    m_ClientWantTarget = false;
}

void CTarget::SendTargetLastTile()
{
    SendTargetTile(m_LastTileGraphic, m_LastTileX, m_LastTileY, m_LastTileZ);
}

void CTarget::SendCancelTarget()
{
    if (!m_Targeting)
    {
        return;
    }

    CPacketTargetCancel(m_Type, m_CursorID, m_CursorType).SendServer();
    m_Targeting = false;
    m_MultiGraphic = 0;
    m_ClientWantTarget = false;
}

void CTarget::SendTargetLastTarget()
{
    SendTargetObject(g_LastTargetObject);
}

} // namespace xuoassist
