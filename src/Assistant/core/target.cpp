// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (assist/target.cpp); see target.h for the
// assistant-owned vs client ::g_Target split. The macro-recording branches
// run through g_MacroExecutor; the tile-hook resolution reads the client's
// own map/statics blocks and tiledata (the Phase-6 replacement for the
// assistant filemanager's GetTile).

#include "target.h"
#include "assist.h"
#include "assistantevents.h"
#include "clientversion.h" // GetClientVersion (the CV_7090 surface-height gate)
#include "packets.h"
#include "../config/assistantconfig.h"
#include "../macros/macroexecutor.h"
#include "../model/gameobjects/gameworld.h"
#include "../model/gameobjects/gameplayer.h"
#include "../core/textparser.h"
#include <xuocore/mulstruct.h>  // MAP_BLOCK / STATICS_BLOCK
#include <xuocore/uodata.h>    // g_Data.m_Static (the client's tiledata)
#include "Managers/MapManager.h" // ::g_MapManager (map/statics block access)

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

namespace {

// The tile flags/name filter of the original CFileManager::GetTile path.
// TNF_BY_GRAPHIC is 0: a bare numeric graphic means
// "match exactly this graphic".
enum
{
    TNF_BY_GRAPHIC = 0x00,
    TNF_MINE = 0x01,
    TNF_TREE = 0x02,
    TNF_WATER = 0x04,
    TNF_LAND = 0x08,
    TNF_ANY = 0x10
};

// One resolved tile (the original CTileInfo record).
struct TARGET_TILE_INFO
{
    ushort Graphic;
    int X;
    int Y;
    int Z;
    bool Land;
};

// CFileManager::TileNameToFlags: "|"-separated tokens;
// the category words OR into the flags, anything else parses as a graphic.
void TargetTileNameToFlags(const string &tileName, ushort &graphic, ushort &flags)
{
    flags = TNF_BY_GRAPHIC;

    CTextParser parser(tileName, "|", "", "");
    vector<string> list = parser.ReadTokens();

    for (const string &str : list)
    {
        if (str == "mine")
        {
            flags |= TNF_MINE;
        }
        else if (str == "tree")
        {
            flags |= TNF_TREE;
        }
        else if (str == "water")
        {
            flags |= TNF_WATER;
        }
        else if (str == "land")
        {
            flags |= TNF_LAND;
        }
        else if (str == "any")
        {
            flags |= TNF_ANY;
        }
        else
        {
            graphic = CAssistant::TextToGraphic(str);
        }
    }
}

// CFileManager::GetTile against the client's own
// map/statics blocks (::g_MapManager - the same access the radar window
// uses). The mine/tree/water category lists lived in the assistant's own
// tile caches (m_MineTiles/m_TreeTiles/m_WaterTiles, fed from Config text
// files that were never part of the ported config); those categories resolve
// like the original's empty-list path.
// DEVIATION: "tree"/"water"/"mine" hooks cannot match by category.
vector<TARGET_TILE_INFO> GetTileAt(int x, int y, int z, const string &tileName)
{
    vector<TARGET_TILE_INFO> result;

    ushort tileID = 0;
    ushort flags = 0;
    TargetTileNameToFlags(tileName, tileID, flags);

    if (EnabledCommandExtendedTargetTile() && g_World != nullptr)
    {
        CIndexMap *index = ::g_MapManager.GetIndex(g_CurrentMap, x >> 3, y >> 3);
        if (index != nullptr && index->MapAddress != 0)
        {
            const int tileX = x & 7;
            const int tileY = y & 7;

            TARGET_TILE_INFO land = { tileID, x, y, z, false };

            MAP_BLOCK *pmb = (MAP_BLOCK *)index->MapAddress;
            const MAP_CELLS &cell = pmb->Cells[(tileY * 8) + tileX];
            land.Graphic = (ushort)(cell.TileID & 0x3FFF);
            land.Z = cell.Z;
            land.Land = true;

            if ((flags & TNF_LAND) != 0)
            {
                // CanBeAdded in by-graphic mode needs an exact graphic match
                // (flags == TNF_BY_GRAPHIC); the LAND bit alone takes the cell
                if (tileID == 0u || (flags == TNF_BY_GRAPHIC && land.Graphic == tileID))
                {
                    result.push_back(land);
                }

                if (result.empty())
                {
                    result.push_back(TARGET_TILE_INFO{ tileID, x, y, z, false });
                }
                return result;
            }

            STATICS_BLOCK *sb = (STATICS_BLOCK *)index->StaticAddress;
            if (sb != nullptr)
            {
                const int count = index->StaticCount;
                for (int c = 0; c < count; c++, sb++)
                {
                    if ((sb->Color != 0u) && sb->Color != 0xFFFF && tileX == sb->X &&
                        tileY == sb->Y)
                    {
                        if ((flags == TNF_BY_GRAPHIC && sb->Color == tileID) ||
                            (flags & TNF_ANY) != 0)
                        {
                            result.push_back(TARGET_TILE_INFO{ sb->Color, x, y, sb->Z, false });
                        }
                    }
                }
            }

            if ((flags & TNF_ANY) != 0 && result.empty())
            {
                result.push_back(land);
            }
        }
    }

    if (result.empty())
    {
        result.push_back(TARGET_TILE_INFO{ tileID, x, y, z, false });
    }

    return result;
}

} // namespace

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

                // The GetTile port + the tiledata
                // surface-height fixup of the original CheckHook: the client's own
                // tiledata (g_Data.m_Static, version-normalized) provides the
                // surface flags/heights.
                int tileZ = z;
                ushort tileGraphic = 0;

                vector<TARGET_TILE_INFO> tileList = GetTileAt(x, y, z, hook.Tile);
                if (!tileList.empty())
                {
                    const TARGET_TILE_INFO &tileInfo = tileList[0];

                    tileGraphic = tileInfo.Graphic;
                    tileZ = tileInfo.Z;

                    if (!tileInfo.Land && GetClientVersion() >= CV_7090 &&
                        ((size_t)tileGraphic < g_Data.m_Static.size()))
                    {
                        const MulStaticTile2 &st = g_Data.m_Static[tileGraphic];

                        if ((st.Flags & 0x00000200) != 0) // IsSurface
                        {
                            tileZ += st.Height;
                        }
                    }
                }
                else
                {
                    tileGraphic = CAssistant::TextToGraphic(hook.Tile);
                }

                SendTargetTile(tileGraphic, x, y, (short)tileZ);
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

        // was g_TabMacros->AddAction(new CMacroWithTargetTile(...)) while a
        // recording was open
        if (g_World != nullptr)
        {
            g_AssistantEvents->OnRecordTargetTile(
                m_LastTileGraphic, m_LastTileX, m_LastTileY, m_LastTileZ);
        }
    }
    else
    {
        reader.Move(5);
        uint serial = reader.ReadUInt32BE();

        ushort graphic = 0;
        ushort color = 0;
        CGameObject *obj = g_World != nullptr ? g_World->FindWorldObject(serial) : nullptr;
        if (obj != nullptr)
        {
            graphic = obj->GetGraphic();
            color = obj->GetColor();
        }

        // was g_TabMacros->AddAction(new CMacroWithObject(...)) while a
        // recording was open; the convert-type
        // tagging happens in the executor
        g_AssistantEvents->OnRecordTargetObject(serial, graphic, color);

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
    m_CurrentClientTarget.resize(reader.Size);
    memcpy(&m_CurrentClientTarget[0], reader.Start, reader.Size);

    // And set the matching values
    m_Type = reader.ReadUInt8();
    m_CursorID = reader.ReadUInt32BE();
    m_CursorType = reader.ReadUInt8();
    m_Targeting = (m_CursorType < 3);
    m_MultiGraphic = 0;
    m_ClientWantTarget = true;

    bool result = (CheckHook() && !m_FromAssist);

    // a playing macro waiting on a target cursor consumes the reply and
    // answers it itself (the original PlayWaiting tap)
    if (result && g_MacroExecutor.IsPlaying() &&
        g_MacroExecutor.PlayWaiting({ MT_WAIT_FOR_TARGET_OBJECT, MT_WAIT_FOR_TARGET_TILE }))
    {
        return false;
    }

    return result;
}

bool CTarget::SetMultiData(CDataReader &reader)
{
    m_CurrentClientTarget.resize(reader.Size);
    memcpy(&m_CurrentClientTarget[0], reader.Start, reader.Size);

    // And set the matching values
    m_Type = reader.ReadUInt8();
    m_CursorID = reader.ReadUInt32BE();
    m_CursorType = reader.ReadUInt8();
    m_Targeting = (m_CursorType < 3);
    m_ClientWantTarget = true;

    reader.Move(10);
    m_MultiGraphic = reader.ReadUInt16BE() + 1;

    bool result = (CheckHook() && !m_FromAssist);

    // the SetRecvData PlayWaiting tap
    if (result && g_MacroExecutor.IsPlaying() &&
        g_MacroExecutor.PlayWaiting({ MT_WAIT_FOR_TARGET_OBJECT, MT_WAIT_FOR_TARGET_TILE }))
    {
        return false;
    }

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
