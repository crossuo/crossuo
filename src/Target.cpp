// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "Target.h"
#include "Config.h"
#include "CrossUO.h"
#include "Multi.h"
#include <xuocore/uodata.h>
#include <xuocore/mulstruct.h>
#include "Network/Packets.h"
#include "Managers/MouseManager.h"
#include "Managers/MapManager.h"
#include "Managers/CustomHousesManager.h"
#include "GameObjects/GameCharacter.h"
#include "GameObjects/CustomHouseMultiObject.h"
#include "Gumps/GumpCustomHouse.h"
#if defined(XUO_ASSISTANT)
#include <core/assist.h> // xuoassist::g_TargetHandler, CAssistant text converters, the armed from-target interception
#include <core/assistantevents.h> // xuoassist::g_AssistantEvents (the macro-recording notifications)
#include <config/assistantconfig.h> // xuoassist::EnabledCommandExtendedTargetTile (the tile-hook world gate)
#include <core/world.h> // ::g_World / ::g_Player (the client world singletons)
#include <macros/macroexecutor.h> // xuoassist::g_MacroExecutor (the PlayWaiting tap)
#include <ctype.h> // tolower (the hook tile-name lowercase)
#include <xuocore/text_parser.h> // ::TextFileParser (the client token parser)
#endif

CTarget g_Target;

CTarget::CTarget()
{
    //Чистимся
    memset(m_Data, 0, sizeof(m_Data));
    memset(m_LastData, 0, sizeof(m_LastData));
}

void CTarget::Reset()
{
    //Чистимся
    memset(m_Data, 0, sizeof(m_Data));
    memset(m_LastData, 0, sizeof(m_LastData));

    if (m_Multi != nullptr)
    {
        delete m_Multi;
        m_Multi = nullptr;
    }

    Type = 0;
    CursorType = 0;
    CursorID = 0;
    Targeting = false;
    MultiGraphic = 0;
#if defined(XUO_ASSISTANT)
    m_HookTargeting = false;
#endif
}

void CTarget::RequestFromCustomHouse()
{
    Type = 2;
    CursorID = 0;
    CursorType = 0;
    Targeting = true;
    MultiGraphic = 0;

    if (g_CustomHouseGump != nullptr)
    {
        g_CustomHouseGump->Erasing = false;
        g_CustomHouseGump->SeekTile = false;
        g_CustomHouseGump->SelectedGraphic = 0;
        g_CustomHouseGump->CombinedStair = false;
        g_CustomHouseGump->WantUpdateContent = true;
    }
}

void CTarget::SetLastTargetObject(int serial)
{
    Type = 0;
    pack32(m_LastData + 7, serial);
}

bool CTarget::SetData(CDataReader &reader)
{
#if defined(XUO_ASSISTANT)
    // The WaitTarget-hook prefix: parses the cursor fields from a private
    // reader over the same buffer (the client body below re-parses them from
    // the untouched shared reader) and lets the hook queue / a pending
    // assistant UI selection / a playing macro consume the reply.
    {
        CDataReader hook(reader.Start, reader.Size);
        hook.Move(1); // the tap dispatcher skipped the fixed-size id byte
        Type = hook.ReadUInt8();
        CursorID = hook.ReadUInt32BE();
        CursorType = hook.ReadUInt8();
        m_HookTargeting = (CursorType < 3);
        MultiGraphic = 0;
        ClientWantTarget = true;
        m_CurrentClientTarget.assign(reader.Start, reader.Start + reader.Size);

        bool result = (CheckHook() && !FromAssist);

        // a playing macro waiting on a target cursor consumes the reply and
        // answers it itself (the original PlayWaiting tap)
        if (result && xuoassist::g_MacroExecutor.IsPlaying() &&
            xuoassist::g_MacroExecutor.PlayWaiting(
                { xuoassist::MT_WAIT_FOR_TARGET_OBJECT, xuoassist::MT_WAIT_FOR_TARGET_TILE }))
        {
            result = false;
        }

        if (!result)
        {
            return false;
        }
    }
#endif

    //Копируем буффер
    memcpy(&m_Data[0], reader.Start, reader.Size);

    //И устанавливаем соответствующие значения
    Type = reader.ReadUInt8();
    CursorID = reader.ReadUInt32BE();
    CursorType = reader.ReadUInt8();
    Targeting = (CursorType < 3);
    MultiGraphic = 0;

    return true;
}

bool CTarget::SetMultiData(CDataReader &reader)
{
#if defined(XUO_ASSISTANT)
    // The SetData WaitTarget-hook prefix: parses the wire cursor fields
    // first (the client body below synthesizes Type=1/CursorType=0 over
    // them on the pass-through path).
    {
        CDataReader hook(reader.Start, reader.Size);
        hook.Move(1); // the tap dispatcher skipped the fixed-size id byte
        Type = hook.ReadUInt8();
        CursorID = hook.ReadUInt32BE();
        CursorType = hook.ReadUInt8();
        m_HookTargeting = (CursorType < 3);
        ClientWantTarget = true;
        m_CurrentClientTarget.assign(reader.Start, reader.Start + reader.Size);

        // the placement graphic (the same offset the client body reads
        // below) before the consume decision (pre-image parity)
        hook.Move(11);
        MultiGraphic = hook.ReadUInt16BE() + 1;

        bool result = (CheckHook() && !FromAssist);

        if (result && xuoassist::g_MacroExecutor.IsPlaying() &&
            xuoassist::g_MacroExecutor.PlayWaiting(
                { xuoassist::MT_WAIT_FOR_TARGET_OBJECT, xuoassist::MT_WAIT_FOR_TARGET_TILE }))
        {
            result = false;
        }

        if (!result)
        {
            return false;
        }
    }
#endif

    //Устанавливаем соответствующие значения
    Type = 1;
    CursorType = 0;
    Targeting = true;
    CursorID = reader.ReadUInt32BE(1);

    //Копируем буффер
    memset(&m_Data[0], 0, 19);
    m_Data[0] = 0x6C;
    m_Data[1] = 1;                           //Таргет на ландшафт
    memcpy(m_Data + 2, reader.Start + 2, 4); //Копируем ID курсора (ID дида)

    reader.ResetPtr();
    reader.Move(18);
    MultiGraphic = reader.ReadUInt16BE() + 1;
    MultiX = reader.ReadUInt16BE();
    MultiY = reader.ReadUInt16BE();

    return true;
}

// The cursor the assistant requested (CAssistant::RequestTarget), as the
// bytes of its former injected 0x6C (assistant packets.cpp
// CPacketToClientTargetRequest): an object-target cursor, CursorID 0x601A,
// CursorType 0. The injected pass ran the full Target handler, so the
// client state, the hook-targeting mirror and the party-helper auto-target
// replay below all ran too; the packet's serial constant at offset 7
// (00 01 00 00 from offset 6) is dead state, every reply overwrites it.
void CTarget::RequestCursor()
{
#if defined(XUO_ASSISTANT)
    memset(m_Data, 0, sizeof(m_Data));
    m_Data[0] = 0x6C;
    pack32(m_Data + 2, 0x0000601A);
    m_Data[7] = 0x01;
    Type = 0;
    CursorID = 0x0000601A;
    CursorType = 0;
    Targeting = true;
    MultiGraphic = 0;
    m_HookTargeting = true;

    if (g_PartyHelperTimer > g_Ticks && (g_PartyHelperTarget != 0u))
    {
        SendTargetObject(g_PartyHelperTarget);
        g_PartyHelperTimer = 0;
        g_PartyHelperTarget = 0;
    }
#endif
}

// The cursor close (the apiscript/actions/macroexecutor cancels), as the
// bytes of the former injected 0x6C type=3 (assistant packets.cpp
// CPacketCancelTarget) through the Target handler: CursorType 3 leaves
// Targeting false. The injected pass ran the full handler, so a pending
// party-helper replay was dropped with the cursor it would have answered
// (its SendTargetObject was already inert at Targeting=false; only the
// pending state cleared).
void CTarget::CancelCursor()
{
#if defined(XUO_ASSISTANT)
    memset(m_Data, 0, sizeof(m_Data));
    m_Data[0] = 0x6C;
    m_Data[6] = 3;
    Type = 0;
    CursorID = 0;
    CursorType = 3;
    Targeting = false;
    MultiGraphic = 0;

    if (g_PartyHelperTimer > g_Ticks && (g_PartyHelperTarget != 0u))
    {
        g_PartyHelperTimer = 0;
        g_PartyHelperTarget = 0;
    }
#endif
}

void CTarget::RestoreCursor(uint8_t *data, int size)
{
    if (data[0] == 0x99)
    {
        // the MultiPlacement handler body (0x99)
        if (g_World == nullptr)
        {
            return;
        }

        CDataReader reader(data, size);
        reader.Move(1); // the tap dispatcher skipped the fixed-size id byte
        SetMultiData(reader);
        return;
    }

    // the Target handler body (0x6C): parse the cursor, then the
    // party-helper auto-target replay
    CDataReader reader(data, size);
    reader.Move(1); // the tap dispatcher skipped the fixed-size id byte
    SetData(reader);

    if (g_PartyHelperTimer > g_Ticks && (g_PartyHelperTarget != 0u))
    {
        SendTargetObject(g_PartyHelperTarget);
        g_PartyHelperTimer = 0;
        g_PartyHelperTarget = 0;
    }
}

void CTarget::SendTargetObject(int serial)
{
    if (!Targeting)
    {
        return; //Если в клиенте нет таргета - выход
    }

    //Пишем серийник объекта, на который ткнули прицелом, остальное - затираем
    pack32(m_Data + 7, serial);
    m_Data[1] = 0;

    CGameObject *obj = (g_World != nullptr ? g_World->FindWorldObject(serial) : nullptr);

    if (obj != nullptr)
    {
        pack16(m_Data + 11, obj->GetX());
        pack16(m_Data + 13, obj->GetY());
        m_Data[15] = 0xFF;
        m_Data[16] = obj->GetZ();
        pack16(m_Data + 17, obj->Graphic);
    }
    else
    {
        pack32(m_Data + 11, 0);
        pack32(m_Data + 15, 0);
    }

    if (serial != g_PlayerSerial)
    {
        g_LastTargetObject = serial;

        //Скопируем для LastTarget
        memcpy(m_LastData, m_Data, sizeof(m_Data));

        if (obj != nullptr && obj->NPC && ((CGameCharacter *)obj)->MaxHits == 0)
        {
            CPacketStatusRequest(serial).Send();
        }
    }

    SendTarget();
}

void CTarget::SendTargetTile(uint16_t tileID, short x, short y, char z)
{
    if (!Targeting)
    {
        return; //Если в клиенте нет таргета - выход
    }

    m_Data[1] = 1;

    //Пишем координаты и индекс тайла, на который ткнули, остальное трем
    pack32(m_Data + 7, 0);
    pack16(m_Data + 11, x);
    pack16(m_Data + 13, y);

    //m_Data[15] = 0xFF;
    //m_Data[16] = z;
    pack16(m_Data + 15, (short)z);
    pack16(m_Data + 17, tileID);

    //Скопируем для LastTarget
    memcpy(m_LastData, m_Data, sizeof(m_Data));

    SendTarget();
}

void CTarget::SendCancelTarget()
{
    if (!Targeting)
    {
        return; //Если в клиенте нет таргета - выход
    }

    //Уходят только нули
    pack32(m_Data + 7, 0);
    pack32(m_Data + 11, 0xFFFFFFFF);
    pack32(m_Data + 15, 0);

    SendTarget();

    if (g_CustomHouseGump != nullptr)
    {
        g_CustomHouseGump->Erasing = false;
        g_CustomHouseGump->SeekTile = false;
        g_CustomHouseGump->SelectedGraphic = 0;
        g_CustomHouseGump->CombinedStair = false;
        g_CustomHouseGump->WantUpdateContent = true;
    }
}

void CTarget::SendLastTarget()
{
    if (!Targeting)
    {
        return; //Если в клиенте нет таргета - выход
    }

    //Восстановим пакет последнего актуального таргета
    memcpy(m_Data, m_LastData, sizeof(m_Data));
    m_Data[0] = 0x6C;
    m_Data[1] = Type;
    m_Data[6] = CursorType;
    pack32(m_Data + 2, CursorID);

    SendTarget();
}

void CTarget::SendTarget()
{
    if (Type != 2)
    {
#if defined(XUO_ASSISTANT)
        // was the 0x6C TargetS send tap (assistant packetmanager): with a
        // from-target hook armed, the raw cursor packet feeds the armed
        // handler first, then the cursor state resets and any
        // assistant-requested cursor re-dispatches through RestoreTarget;
        // the reply was never dropped (the tap's "consumed" result had no
        // send-side effect), so it still goes out here. Without an armed
        // hook the reply records - the SetSendData body runs inlined in the
        // else branch, covering every SendTargetObject/SendTargetTile/
        // SendCancelTarget/SendLastTarget producer at this single choke point.
        if (xuoassist::g_TargetHandler != nullptr)
        {
            (xuoassist::g_Assistant.*(xuoassist::g_TargetHandler))(m_Data);
            xuoassist::g_TargetHandler = nullptr;
            // was the assistant CTarget::Reset: the target-state mirror fields
            // only. The client Reset() would memset m_Data/m_LastData and the
            // send below would go out as 19 zero bytes, losing the from-target
            // answer and the click snapshot.
            Type = 0;
            CursorType = 0;
            CursorID = 0;
            Targeting = false;
            MultiGraphic = 0;
            m_HookTargeting = false;
            RestoreTarget();
        }
        else
        {
            // was the 0x6C TargetS send tap recording (xuoassist
            // CTarget::SetSendData): every producer that reaches SendTarget
            // without an armed hook is recorded (the macro target round +
            // the LastTarget mirror)
            m_HookTargeting = false;
            ClientWantTarget = false;

            CDataReader reader(m_Data, sizeof(m_Data));
            reader.Move(1); // the tap dispatcher skipped the fixed-size id byte
            if (reader.ReadUInt8() == 1) // tile
            {
                reader.Move(9);
                LastTileX = reader.ReadUInt16BE();
                LastTileY = reader.ReadUInt16BE();
                LastTileZ = reader.ReadUInt16BE();
                LastTileGraphic = reader.ReadUInt16BE();

                // was g_TabMacros->AddAction(new CMacroWithTargetTile(...)) while a
                // recording was open
                if (g_World != nullptr)
                {
                    xuoassist::g_AssistantEvents->OnRecordTargetTile(
                        LastTileGraphic, LastTileX, LastTileY, LastTileZ);
                }
            }
            else
            {
                reader.Move(5);
                uint32_t serial = reader.ReadUInt32BE();

                uint16_t graphic = 0;
                uint16_t color = 0;
                CGameObject *obj = g_World != nullptr ? g_World->FindWorldObject(serial) : nullptr;
                if (obj != nullptr)
                {
                    graphic = obj->Graphic;
                    color = obj->Color;
                }

                // was g_TabMacros->AddAction(new CMacroWithObject(...)) while a
                // recording was open; the convert-type
                // tagging happens in the executor
                xuoassist::g_AssistantEvents->OnRecordTargetObject(serial, graphic, color);

                if (serial != 0u && serial != g_PlayerSerial)
                {
                    g_LastTargetObject = serial;
                }
            }
        }
#endif
        g_Game.Send(m_Data, sizeof(m_Data));
    }

    memset(m_Data, 0, sizeof(m_Data));
    Targeting = false;
    MultiGraphic = 0;

    g_MouseManager.CancelDoubleClick = true;
}

void CTarget::UnloadMulti()
{
    if (m_Multi != nullptr)
    {
        delete m_Multi;
        m_Multi = nullptr;
    }
}

void CTarget::LoadMulti(int offsetX, int offsetY, char offsetZ)
{
    UnloadMulti();

    CIndexMulti &index = g_Index.m_Multi[MultiGraphic - 1];
    int count = (int)index.Count;
    if (index.UopBlock != nullptr)
    {
        std::vector<uint8_t> data = g_FileManager.m_MultiCollection.GetData(index.UopBlock);
        if (data.empty())
        {
            return;
        }

        CDataReader reader(&data[0], data.size());
        reader.Move(8); //ID + Count
        for (int i = 0; i < count; i++)
        {
            const uint16_t graphic = reader.ReadUInt16LE();
            const int16_t x = reader.ReadInt16LE();
            const int16_t y = reader.ReadInt16LE();
            const int8_t z = (int8_t)reader.ReadInt16LE();
            const uint16_t _flags = reader.ReadUInt16LE();
            (void)_flags;
            const uint32_t clilocsCount = reader.ReadUInt32LE();
            if (clilocsCount != 0)
            {
                reader.Move(clilocsCount * 4);
            }

            auto mo = new CMultiObject(graphic, x + offsetX, y + offsetY, z + (char)offsetZ, 2);
            g_MapManager.AddRender(mo);
            AddMultiObject(mo);
        }
    }
    else if (index.Address != 0)
    {
        int itemOffset = sizeof(MULTI_BLOCK);

        if (g_Config.ClientVersion >= CV_7090)
        {
            itemOffset = sizeof(MULTI_BLOCK_NEW);
        }

        for (int j = 0; j < count; j++)
        {
            MULTI_BLOCK *pmb = (MULTI_BLOCK *)(index.Address + (j * itemOffset));

            CMultiObject *mo = new CMultiObject(
                pmb->ID, offsetX + pmb->X, offsetY + pmb->Y, offsetZ + (char)pmb->Z, 2);
            g_MapManager.AddRender(mo);
            AddMultiObject(mo);
        }
    }
}

void CTarget::AddMultiObject(CMultiObject *obj)
{
    if (m_Multi == nullptr)
    {
        m_Multi = new CMulti(obj->GetX(), obj->GetY());
        m_Multi->m_Next = nullptr;
        m_Multi->m_Prev = nullptr;
        m_Multi->m_Items = obj;
        obj->m_Next = nullptr;
        obj->m_Prev = nullptr;
    }
    else
    {
        CMulti *multi = GetMultiAtXY(obj->GetX(), obj->GetY());

        if (multi != nullptr)
        {
            for (CMultiObject *multiobj = (CMultiObject *)multi->m_Items; multiobj != nullptr; multiobj = (CMultiObject *)multiobj->m_Next)
            {
                if (obj->GetZ() < multiobj->GetZ())
                {
                    if (multiobj->m_Prev == nullptr)
                    {
                        multi->Insert(multiobj->m_Prev, obj);
                    }
                    else
                    {
                        multi->Insert(multiobj, obj);
                    }

                    return;
                }

                if (multiobj->m_Next == nullptr)
                {
                    multiobj->m_Next = obj;
                    obj->m_Prev = multiobj;
                    obj->m_Next = nullptr;

                    return;
                }
            }

            //Если пришли сюда - что-то пошло не так
        }
        else
        {
            // FIXME: potential leak, validate
            CMulti *newmulti = new CMulti(obj->GetX(), obj->GetY());
            newmulti->m_Next = nullptr;
            newmulti->m_Items = obj;
            obj->m_Next = nullptr;
            obj->m_Prev = nullptr;

            multi = m_Multi;

            while (multi != nullptr)
            {
                if (multi->m_Next == nullptr)
                {
                    multi->m_Next = newmulti;
                    newmulti->m_Prev = multi;
                    break;
                }

                multi = (CMulti *)multi->m_Next;
            }
        }
    }
}

CMulti *CTarget::GetMultiAtXY(short x, short y)
{
    CMulti *multi = m_Multi;

    while (multi != nullptr)
    {
        if (multi->X == x && multi->Y == y)
        {
            break;
        }

        multi = (CMulti *)multi->m_Next;
    }

    return multi;
}

#if defined(XUO_ASSISTANT)
// The WaitTarget hook queue and the consume-path senders (the former
// xuoassist::CTarget): the hook answers and the macro PlayWaiting answers go
// out as raw 0x6C packets from here, while the client Targeting stays false.

CTargetHook::CTargetHook(const uint32_t &serial)
{
    TargetOnObject = true;
    Serial = serial;
}

CTargetHook::CTargetHook(const astr_t &tile, const int &x, const int &y, const int &z, const bool &relative)
    : Tile(tile)
{
    TargetOnObject = false;
    X = x;
    Y = y;
    Z = z;
    Relative = relative;
}

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
    uint16_t Graphic;
    int X;
    int Y;
    int Z;
    bool Land;
};

// CFileManager::TileNameToFlags: "|"-separated tokens;
// the category words OR into the flags, anything else parses as a graphic.
void TargetTileNameToFlags(const astr_t &tileName, uint16_t &graphic, uint16_t &flags)
{
    flags = TNF_BY_GRAPHIC;

    TextFileParser parser({}, "|", "", "");
    std::vector<astr_t> list = parser.GetTokens(tileName);

    for (const astr_t &str : list)
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
            graphic = xuoassist::CAssistant::TextToGraphic(str);
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
std::vector<TARGET_TILE_INFO> GetTileAt(int x, int y, int z, const astr_t &tileName)
{
    std::vector<TARGET_TILE_INFO> result;

    uint16_t tileID = 0;
    uint16_t flags = 0;
    TargetTileNameToFlags(tileName, tileID, flags);

    if (xuoassist::EnabledCommandExtendedTargetTile() && g_World != nullptr)
    {
        CIndexMap *index = g_MapManager.GetIndex(g_CurrentMap, x >> 3, y >> 3);
        if (index != nullptr && index->MapAddress != 0)
        {
            const int tileX = x & 7;
            const int tileY = y & 7;

            TARGET_TILE_INFO land = { tileID, x, y, z, false };

            MAP_BLOCK *pmb = (MAP_BLOCK *)index->MapAddress;
            const MAP_CELLS &cell = pmb->Cells[(tileY * 8) + tileX];
            land.Graphic = (uint16_t)(cell.TileID & 0x3FFF);
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

void CTarget::SetObjectsHook(const std::vector<astr_t> &objects)
{
    m_WaitTargets.clear();

    if (!objects.empty())
    {
        for (const astr_t &i : objects)
        {
            m_WaitTargets.push_back(CTargetHook(xuoassist::CAssistant::TextToSerial(i)));
        }
    }
}

void CTarget::SetTileHook(
    const astr_t &tile, const int &x, const int &y, const int &z, const bool &relative)
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
            SendHookTargetObject(hook.Serial);
        }
        else
        {
            astr_t tileName = str_lower(hook.Tile);

            if (tileName == "lasttile")
            {
                SendHookTargetLastTile();
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
                uint16_t tileGraphic = 0;

                std::vector<TARGET_TILE_INFO> tileList = GetTileAt(x, y, z, hook.Tile);
                if (!tileList.empty())
                {
                    const TARGET_TILE_INFO &tileInfo = tileList[0];

                    tileGraphic = tileInfo.Graphic;
                    tileZ = tileInfo.Z;

                    if (!tileInfo.Land && g_Config.ClientVersion >= CV_7090 &&
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
                    tileGraphic = xuoassist::CAssistant::TextToGraphic(hook.Tile);
                }

                SendHookTargetTile(tileGraphic, x, y, (short)tileZ);
            }
        }

        m_WaitTargets.erase(m_WaitTargets.begin());

        return false;
    }

    return true;
}

void CTarget::RestoreTarget()
{
    if (FromAssist)
    {
        FromAssist = false;

        if (ClientWantTarget)
        {
            RestoreCursor(&m_CurrentClientTarget[0], (int)m_CurrentClientTarget.size());
        }
    }
}

void CTarget::SendHookTargetObject(const uint32_t &serial)
{
    if (!m_HookTargeting)
    {
        return;
    }

    CPacketTargetObject(0, CursorID, CursorType, serial).Send();
    m_HookTargeting = false;
    MultiGraphic = 0;
    ClientWantTarget = false;
}

void CTarget::SendHookTargetTile(const uint16_t tileID, const short &x, const short &y, short z)
{
    if (!m_HookTargeting)
    {
        return;
    }

    CPacketTargetTile(1, CursorID, CursorType, tileID, x, y, z).Send();
    m_HookTargeting = false;
    MultiGraphic = 0;
    ClientWantTarget = false;
}

void CTarget::SendHookTargetLastTile()
{
    SendHookTargetTile(LastTileGraphic, LastTileX, LastTileY, LastTileZ);
}

void CTarget::SendHookCancelTarget()
{
    if (!m_HookTargeting)
    {
        return;
    }

    CPacketTargetCancel(Type, CursorID, CursorType).Send();
    m_HookTargeting = false;
    MultiGraphic = 0;
    ClientWantTarget = false;
}

#endif // XUO_ASSISTANT
