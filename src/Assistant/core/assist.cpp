// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (assist/assist.cpp), trimmed per assistant-plan.md
// §4.1. Method behavior is identical; the deltas are:
//  - the signal/slot marshaling is gone (single-threaded, §8.1);
//  - the data-list lookups (types/objects/find/ignore/friends/enemies) go to
//    the AssistantConfig module, which owns the data the original lists displayed;
//  - numeric-option lookups (distance/delay/messages level) go to
//    AssistantConfig too (was g_TabMain / g_AssistantForm);
//  - the from-target handlers that filled UI widgets keep the world/state
//    half and route the UI half through the events seam (Phase 2).

#include "assist.h"
#include "../Misc.h" // unpack16/unpack32 (the client byte-order helpers)
#include "../script/scriptgump.h" // the text window dump (the g_TextDialog port)
#include "Network/Packets.h" // ::CPacketStatusRequest and the other client send builders
#include "Managers/PacketManager.h"      // ::g_PacketManager (DisplayTalkTextW, DeleteObject)
#include "textconv.h"
#include "target.h"
#include "Target.h" // ::g_Target (the client target cursor: RequestCursor)
#include "assistantevents.h"
#include "world.h"
#include "worldutil.h"
#include "textconv.h"
#include "textcommandmanager.h" // g_TextCommandManager (the command-prefix intercept)
#include "clientversion.h" // GetClientVersion (the negotiated client version)
#include "../config/assistantconfig.h"
#include "../macros/macroexecutor.h" // RecordingSuspension (assistant-initiated sends do not self-record)
#include "CrossUO.h" // ::g_Game (the client send bridge)
#include <common/str.h>
#include <xuocore/text_parser.h> // ::TextFileParser (the client token parser)
#include <common/logging/logging.h>
#include <stdlib.h>

namespace xuoassist {

static astr_t to_lower_copy(astr_t text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }
    return text;
}

CAssistant g_Assistant;
TARGET_HANDLER g_TargetHandler = nullptr;
MOVITEM_DATA g_MoveItemData;

CAssistant::CAssistant() {}

CAssistant::~CAssistant() {}

void CAssistant::ScriptError(const astr_t &title, const astr_t &text)
{
    // Was CAssistant::slot_ScriptError.
    ClientPrint(title, 0x0021);
    ClientPrint(text, 0x0021);
}

uint CAssistant::RawStringToUInt(const astr_t &value)
{
    uint result = 0;

    if (value.length() > 1)
    {
        if (value.at(0) == '#')
        {
            result = (uint)strtoul(("0x" + value.substr(1)).c_str(), nullptr, 16);
        }
        else if (value == "red")
        {
            result = 0xFF0000FF;
        }
        else if (value == "cyan")
        {
            result = 0x00FFFFFF;
        }
        else if (value == "blue")
        {
            result = 0x0000FFFF;
        }
        else if (value == "darkblue")
        {
            result = 0x0000A0FF;
        }
        else if (value == "lightblue")
        {
            result = 0xADD8E6FF;
        }
        else if (value == "purple")
        {
            result = 0x800080FF;
        }
        else if (value == "yellow")
        {
            result = 0x00FFFFFF;
        }
        else if (value == "lime")
        {
            result = 0x00FF00FF;
        }
        else if (value == "magenta")
        {
            result = 0xFF00FFFF;
        }
        else if (value == "white")
        {
            result = 0xFFFEFEFF;
        }
        else if (value == "silver")
        {
            result = 0xC0C0C0FF;
        }
        else if (value == "gray" || value == "grey")
        {
            result = 0x808080FF;
        }
        else if (value == "black")
        {
            result = 0x010101FF;
        }
        else if (value == "orange")
        {
            result = 0xFFA500FF;
        }
        else if (value == "brown")
        {
            result = 0xA52A2AFF;
        }
        else if (value == "maroon")
        {
            result = 0x800000FF;
        }
        else if (value == "green")
        {
            result = 0x008000FF;
        }
        else if (value == "olive")
        {
            result = 0x808000FF;
        }
    }

    return result;
}

bool CAssistant::RawStringToBool(astr_t value)
{
    value = to_lower_copy(value);
    bool result = false;

    if (value == "true" || value == "on")
    {
        result = true;
    }
    else
    {
        result = (atoi(value.c_str()) != 0);
    }

    return result;
}

uint CAssistant::TextToUInt(const astr_t &text)
{
    return ::xuoassist::TextToUInt(text); // core/textconv (shared with the model)
}

uint CAssistant::TextToSerial(astr_t text)
{
    return ::xuoassist::TextToSerial(to_lower_copy(text));
}

ushort CAssistant::TextToGraphic(astr_t text)
{
    return ::xuoassist::TextToGraphic(to_lower_copy(text));
}

int CAssistant::TextToLayer(astr_t text)
{
    text = to_lower_copy(text);

    for (int i = 0; i < 30; i++)
    {
        if (text == to_lower_copy(g_LayerName[i]))
        {
            return i;
        }
    }

    return atoi(text.c_str());
}

astr_t CAssistant::ColorToText(const uint &value)
{
    char buf[64] = { 0 };
    snprintf(buf, sizeof(buf), "#%08X", value);
    return buf;
}

astr_t CAssistant::SerialToText(const uint &value)
{
    return ::xuoassist::SerialToText(value);
}

astr_t CAssistant::GraphicToText(const ushort &value)
{
    return ::xuoassist::GraphicToText(value);
}

astr_t CAssistant::BoolToText(const bool &value)
{
    return value ? "true" : "false";
}

void CAssistant::ClientPrint(const astr_t &text, const ushort &color)
{
    g_PacketManager.DisplayTalkTextW(
        0xFFFFFFFF, nullptr, "X:UO Assist", wstr_from(text), color, ST_NORMAL);
}

void CAssistant::ClientCharPrint(const uint &serial, const astr_t &text, const ushort &color)
{
    g_PacketManager.DisplayTalkTextW(
        serial,
        (g_World != nullptr ? g_World->FindWorldObject(serial) : nullptr),
        "X:UO Assist",
        wstr_from(text),
        color,
        ST_NORMAL);
}

void CAssistant::ClientPrintDebugLevel1(const astr_t &text, const ushort &color)
{
    if (g_AssistantConfig.GetMessagesLevel() >= 1)
    {
        ClientPrint(text, color);
    }
}

void CAssistant::ClientPrintDebugLevel2(const astr_t &text, const ushort &color)
{
    if (g_AssistantConfig.GetMessagesLevel() >= 2)
    {
        ClientPrint(text, color);
    }
}

void CAssistant::RequestTarget(const astr_t &text)
{
    if (text.length() != 0)
    {
        ClientPrintDebugLevel2(text);
    }

    g_Target.FromAssist = true;
    ::g_Target.RequestCursor();
}

void CAssistant::LogOut()
{
    ::g_Game.LogOut();
}

void CAssistant::DumpObjectInfo(CGameObject *obj)
{
    if (obj == nullptr)
    {
        return;
    }

    if (g_AssistantConfig.ObjectInspector())
    {
        g_AssistantEvents->OnInspectorUpdate(obj->Serial);
        return;
    }

    // the g_TextDialog->AddText + show/activateWindow body is restored by the
    // 4b-2 text window below; the same text goes through the OnJournalMessage
    // seam (the original app did both)
    int layer = 0;
    if (!obj->NPC)
    {
        layer = ((CGameItem *)obj)->Layer;
    }

    char text[512] = { 0 };
    snprintf(
        text,
        sizeof(text),
        "Serial=0x%08X Graphic=0x%04X Name=%s\nCount: %i  Color: 0x%04X  Layer: %i\nX=%i Y=%i Z=%i C=0x%08X F=0x%02X\n",
        obj->Serial,
        obj->Graphic,
        obj->GetName().c_str(),
        obj->Count,
        obj->Color,
        layer,
        obj->GetX(),
        obj->GetY(),
        obj->GetZ(),
        obj->Container,
        obj->GetFlags());

    CJournalMessage message(0xFFFFFFFF, g_MessagesColor, text);
    message.Flags = JMT_SYSTEM;
    g_AssistantEvents->OnJournalMessage(message);

    // the g_TextDialog->ClearText/AddText(text) + show port (scriptgump.cpp,
    // the 4b-2 text window) - the same dump the original dialog showed
    ScriptTextWindow_Command(TW_CLEAR, "", 0, 0, "");
    ScriptTextWindow_Command(TW_PRINT, text, 0, 0, "");
    ScriptTextWindow_Command(TW_OPEN, "", 0, 0, "");
}

void CAssistant::DumpTileInfo(const ushort &graphic, const short &x, const short &y, const char &z)
{
    char text[64] = { 0 };
    snprintf(text, sizeof(text), "0x%04X %i %i %i\n", graphic, x, y, z);

    CJournalMessage message(0xFFFFFFFF, g_MessagesColor, text);
    message.Flags = JMT_SYSTEM;
    g_AssistantEvents->OnJournalMessage(message);

    // the g_TextDialog body port (see DumpObjectInfo)
    ScriptTextWindow_Command(TW_CLEAR, "", 0, 0, "");
    ScriptTextWindow_Command(TW_PRINT, text, 0, 0, "");
    ScriptTextWindow_Command(TW_OPEN, "", 0, 0, "");
}

void CAssistant::GetStatus(const uint &serial)
{
    g_LastStatusRequest = serial;
    ::CPacketStatusRequest(serial).Send();
}

void CAssistant::Click(const uint &serial)
{
    ::CPacketClickRequest(serial).Send();
}

void CAssistant::DoubleClick(const uint &serial)
{
    g_LastUseObject = serial;
    g_LastUseObjectTimer = g_Ticks;
    // assistant-initiated uses must not record themselves; the client's
    // CGame::DoubleClick is the recording tap
    RecordingSuspension suspension;

    g_Game.DoubleClick(serial);
}

void CAssistant::Attack(const uint &serial)
{
    // assistant-initiated attacks must not record themselves; the client's
    // CGame::AttackReq is the recording tap
    RecordingSuspension suspension;

    g_Game.AttackReq(serial);
}

uchar CAssistant::GetNotorietyFlags(const astr_t &text)
{
    uchar flags = NTF_ALL;

    TextFileParser parser({}, "| ", "", "");

    vector<astr_t> list = parser.GetTokens(to_lower_copy(text));

    for (const astr_t &str : list)
    {
        if (str == "innocent" || str == "blue")
        {
            flags |= NTF_INNOCENT;
        }
        else if (str == "friendly" || str == "green")
        {
            flags |= NTF_FRIENDLY;
        }
        else if (str == "gray")
        {
            flags |= NTF_SOMEONE_GRAY;
        }
        else if (str == "criminal")
        {
            flags |= NTF_CRIMINAL;
        }
        else if (str == "enemy" || str == "orange")
        {
            flags |= NTF_ENEMY;
        }
        else if (str == "murderer" || str == "red")
        {
            flags |= NTF_MURDERER;
        }
        else if (str == "invulnerable" || str == "yellow")
        {
            flags |= NTF_INVULNERABLE;
        }
    }

    return flags;
}

CGameObject *CAssistant::FindList(
    const astr_t &findListName,
    const astr_t &containerText,
    const astr_t &distanceText,
    const ushort &flags,
    const astr_t &notorietyText,
    bool recurseSearch,
    vector<astr_t> &result)
{
    vector<CFindItem> findListItems;

    TextFileParser findListNameParser({}, "|", "", "");

    vector<astr_t> listNames = findListNameParser.GetTokens(findListName);

    for (const astr_t &str : listNames)
    {
        CFindListEntry *list = g_AssistantConfig.GetFindList(str);

        if (list != nullptr)
        {
            vector<CFindItem> &temp = list->m_Items;
            for (const CFindItem &i : temp)
            {
                findListItems.push_back(i);
            }
        }
    }

    vector<CIgnoreItem> ignoreListItems;
    TextFileParser ignoreListNameParser({}, "|", "", "");

    listNames = ignoreListNameParser.GetTokens(m_UsedIgnoreList);

    for (const astr_t &str : listNames)
    {
        CIgnoreListEntry *list = g_AssistantConfig.GetIgnoreList(str);

        if (list != nullptr)
        {
            vector<CIgnoreItem> &temp = list->m_Items;
            for (const CIgnoreItem &i : temp)
            {
                ignoreListItems.push_back(i);
            }
        }
    }

    uchar notoriety = GetNotorietyFlags(notorietyText);
    uint container = TextToSerial(containerText);
    int distance = g_AssistantConfig.DistanceFromText(distanceText);
    CGameObject *found = nullptr;

    if (!findListItems.empty())
    {
        for (const CFindItem &item : findListItems)
        {
            CGameObject *temp =
                FindType(item, ignoreListItems, container, distance, flags, notoriety, recurseSearch, result);

            if (temp != nullptr)
            {
                if ((flags & FTF_FAST) != 0)
                {
                    return temp;
                }
                if ((flags & FTF_NEAREST) != 0)
                {
                    if (found == nullptr || GetDistance(g_Player, temp) < GetDistance(g_Player, found))
                    {
                        found = temp;
                    }
                }
                else
                {
                    found = temp;
                }
            }
        }
    }

    if ((flags & FTF_NEAREST) != 0)
    {
        result.clear();

        if (found != nullptr)
        {
            result.push_back(CAssistant::SerialToText(found->Serial));
        }
    }

    return found;
}

CGameObject *CAssistant::FindType(
    const vector<astr_t> &graphic,
    const vector<astr_t> &color,
    const astr_t &containerText,
    const astr_t &distanceText,
    const ushort &flags,
    const astr_t &notorietyText,
    bool recurseSearch,
    vector<astr_t> &result)
{
    result.clear();

    if (g_World == nullptr)
    {
        return nullptr;
    }

    vector<CIgnoreItem> ignoreListItems;
    TextFileParser ignoreListNameParser({}, "|", "", "");

    vector<astr_t> listNames = ignoreListNameParser.GetTokens(m_UsedIgnoreList);

    for (const astr_t &str : listNames)
    {
        CIgnoreListEntry *list = g_AssistantConfig.GetIgnoreList(str);

        if (list != nullptr)
        {
            vector<CIgnoreItem> &temp = list->m_Items;
            for (const CIgnoreItem &i : temp)
            {
                ignoreListItems.push_back(i);
            }
        }
    }

    uchar notoriety = GetNotorietyFlags(notorietyText);
    uint container = TextToSerial(containerText);
    int distance = g_AssistantConfig.DistanceFromText(distanceText);
    int graphicSize = (int)graphic.size();
    int colorSize = (int)color.size();
    CGameObject *found = nullptr;

    for (int i = 0; i < graphicSize; i++)
    {
        for (int j = 0; j < colorSize; j++)
        {
            CGameObject *temp = FindType(
                CFindItem(graphic[i], color[j]),
                ignoreListItems,
                container,
                distance,
                flags,
                notoriety,
                recurseSearch,
                result);

            if (temp != nullptr)
            {
                if ((flags & FTF_FAST) != 0)
                {
                    return temp;
                }
                if ((flags & FTF_NEAREST) != 0)
                {
                    if (found == nullptr ||
                        GetDistance(g_Player, temp) < GetDistance(g_Player, found))
                    {
                        found = temp;
                    }
                }
                else
                {
                    found = temp;
                }
            }
        }
    }

    if ((flags & FTF_NEAREST) != 0)
    {
        result.clear();

        if (found != nullptr)
        {
            result.push_back(CAssistant::SerialToText(found->Serial));
        }
    }

    return found;
}

CGameObject *CAssistant::FindType(
    const CFindItem &findListItem,
    const vector<CIgnoreItem> &ignoreList,
    const uint &container,
    const int &distance,
    ushort flags,
    const uchar &notoriety,
    bool recurseSearch,
    vector<astr_t> &list)
{
    if (g_World == nullptr)
    {
        return nullptr;
    }

    CGameObject *start = nullptr;
    CGameObject *result = nullptr;
    bool checkDistance = false;
    bool mobileOnly = false;
    bool itemsOnly = false;
    bool humanOnly = false;
    bool liveOnly = false;
    bool deadOnly = false;
    bool ignoreFriends = false;
    bool ignoreEnemies = false;

    if (container == 0xFFFFFFFF)
    {
        start = g_World->m_Items;
        checkDistance = true;
        recurseSearch = false;

        mobileOnly = ((flags & FTF_MOBILE) != 0);
        itemsOnly = ((flags & FTF_ITEM) != 0);
        humanOnly = ((flags & FTF_HUMAN) != 0);
        liveOnly = ((flags & FTF_LIVE) != 0);
        deadOnly = ((flags & FTF_DEAD) != 0);
        ignoreFriends = ((flags & FTF_IGNORE_FRIENDS) != 0);
        ignoreEnemies = ((flags & FTF_IGNORE_ENEMIES) != 0);
    }
    else
    {
        CGameObject *obj = g_World->FindWorldObject(container);

        if (obj != nullptr)
        {
            if (obj->IsCorpse() && !EnabledCommandSearchCorpse())
            {
                start = nullptr;
            }
            else
            {
                start = (CGameObject *)obj->m_Items;
            }
        }

        if ((flags & FTF_NEAREST) != 0)
        {
            flags &= ~FTF_NEAREST;
        }
    }

    vector<CGameObject *> containers;

    for (CGameObject *item = (CGameObject *)start; item != nullptr; item = (CGameObject *)item->m_Next)
    {
        if (!item->Ignored && (!checkDistance || GetDistance(g_Player, item) <= distance) &&
            findListItem.Found(item->Graphic, item->Color))
        {
            if (mobileOnly && !item->NPC)
            {
                continue;
            }
            if (item->NPC)
            {
                if (itemsOnly)
                {
                    continue;
                }
                if (humanOnly && !item->IsHuman())
                {
                    continue;
                }

                bool dead = ((CGameCharacter *)item)->IsDead();
                if (liveOnly && dead)
                {
                    continue;
                }
                if (deadOnly && !dead)
                {
                    continue;
                }
                if (ignoreFriends && g_AssistantConfig.InFriendList(item->Serial))
                {
                    continue;
                }
                if (ignoreEnemies && g_AssistantConfig.InEnemyList(item->Serial))
                {
                    continue;
                }
            }

            bool ignored = false;

            if (!ignoreList.empty())
            {
                for (const CIgnoreItem &ii : ignoreList)
                {
                    if (ii.Found(item->Serial, item->Graphic, item->Color))
                    {
                        ignored = true;
                        break;
                    }
                }
            }

            if (!ignored && (notoriety != 0u))
            {
                if (item->NPC)
                {
                    ignored = true;
                    uchar itemNoto = ((CGameCharacter *)item)->Notoriety;

                    if (((notoriety & NTF_INNOCENT) != 0) && itemNoto == NT_INNOCENT)
                    {
                        ignored = false;
                    }
                    else if (((notoriety & NTF_FRIENDLY) != 0) && itemNoto == NT_FRIENDLY)
                    {
                        ignored = false;
                    }
                    else if (((notoriety & NTF_SOMEONE_GRAY) != 0) && itemNoto == NT_SOMEONE_GRAY)
                    {
                        ignored = false;
                    }
                    else if (((notoriety & NTF_CRIMINAL) != 0) && itemNoto == NT_CRIMINAL)
                    {
                        ignored = false;
                    }
                    else if (((notoriety & NTF_ENEMY) != 0) && itemNoto == NT_ENEMY)
                    {
                        ignored = false;
                    }
                    else if (((notoriety & NTF_MURDERER) != 0) && itemNoto == NT_MURDERER)
                    {
                        ignored = false;
                    }
                    else if (((notoriety & NTF_INVULNERABLE) != 0) && itemNoto == NT_INVULNERABLE)
                    {
                        ignored = false;
                    }
                }
            }

            if (!ignored)
            {
                list.push_back(CAssistant::SerialToText(item->Serial));

                if ((flags & FTF_NEAREST) != 0)
                {
                    if (result == nullptr ||
                        GetDistance(g_Player, item) < GetDistance(g_Player, result))
                    {
                        result = item;
                    }
                }
                else
                {
                    result = item;

                    if ((flags & FTF_FAST) != 0)
                    {
                        break;
                    }
                }
            }
        }

        if (recurseSearch && !item->Empty() &&
            (item->NPC || ((CGameItem *)item)->Layer != OL_BANK))
        {
            containers.push_back(item);
        }
    }

    if (((flags & FTF_FAST) == 0) || result == nullptr)
    {
        for (CGameObject *item : containers)
        {
            CGameObject *temp = xuoassist::FindType(item, findListItem, ignoreList, flags, recurseSearch, list);

            if (temp != nullptr)
            {
                result = temp;

                if ((flags & FTF_FAST) != 0)
                {
                    break;
                }
            }
        }
    }

    return result;
}

int CAssistant::CountType(
    const vector<astr_t> &graphic,
    const vector<astr_t> &color,
    const astr_t &containerText,
    const astr_t &distanceText,
    bool recurseSearch)
{
    if (g_World == nullptr)
    {
        return 0;
    }

    uint container = TextToSerial(containerText);
    int distance = g_AssistantConfig.DistanceFromText(distanceText);
    int graphicSize = (int)graphic.size();
    int colorSize = (int)color.size();
    int count = 0;

    for (int i = 0; i < graphicSize; i++)
    {
        for (int j = 0; j < colorSize; j++)
        {
            count += CountType(CFindItem(graphic[i], color[j]), container, distance, recurseSearch);
        }
    }

    return count;
}

int CAssistant::CountType(
    const CFindItem &findListItem, const uint &container, const int &distance, bool recurseSearch)
{
    if (g_World == nullptr)
    {
        return 0;
    }

    CGameObject *start = nullptr;
    int count = 0;
    bool checkDistance = false;

    if (container == 0xFFFFFFFF)
    {
        start = g_World->m_Items;
        checkDistance = true;
        recurseSearch = false;
    }
    else
    {
        CGameObject *obj = g_World->FindWorldObject(container);

        if (obj != nullptr)
        {
            start = (CGameObject *)obj->m_Items;
        }
    }

    for (CGameObject *item = (CGameObject *)start; item != nullptr; item = (CGameObject *)item->m_Next)
    {
        if (findListItem.Found(item->Graphic, item->Color) &&
            (!checkDistance || GetDistance(g_Player, item) <= distance))
        {
            count += item->Count;
        }

        if (recurseSearch && !item->Empty() &&
            (item->NPC || ((CGameItem *)item)->Layer != OL_BANK))
        {
            count += CountItems(item, findListItem, recurseSearch);
        }
    }

    return count;
}

void CAssistant::UseType(
    const CFindItem &findListItem, const uint &container, const int &distance, const bool &recurseSearch)
{
    vector<astr_t> list;

    CGameObject *item =
        FindType(findListItem, vector<CIgnoreItem>(), container, distance, FTF_NORMAL, 0, recurseSearch, list);

    if (item != nullptr)
    {
        DoubleClick(item->Serial);
    }
}

void CAssistant::MoveItem(CGameItem *item)
{
    // The original app sent the pickup/drop pair directly (no client-side
    // use-action queue); the client's ObjectOnCursor flow is untouched.
    if (item == nullptr)
    {
        return;
    }

    uint serial = item->Serial;

    if (g_MoveItemData.Count < 1)
    {
        g_MoveItemData.Count = item->Count;
    }

    ::CPacketPickupRequest(serial, g_MoveItemData.Count).Send();

    if (GetClientVersion() >= CV_6017)
    {
        ::CPacketDropRequestNew(
            serial, g_MoveItemData.X, g_MoveItemData.Y, g_MoveItemData.Z, 0, g_MoveItemData.Container)
            .Send();
    }
    else
    {
        ::CPacketDropRequestOld(
            serial, g_MoveItemData.X, g_MoveItemData.Y, g_MoveItemData.Z, g_MoveItemData.Container)
            .Send();
    }
}

void CAssistant::MoveEquip(const uint &serial, const int &layer, const uint &container)
{
    ::CPacketPickupRequest(serial, 1).Send();
    ::CPacketEquipRequest(serial, layer, container).Send();
}

void CAssistant::Track(const bool &state, short x, short y)
{
    if (x < 0)
    {
        x = g_Player->GetX();
    }

    if (y < 0)
    {
        y = g_Player->GetY();
    }

    // the old and new 0xBA layouts differ only in the trailing serial field
    // the client ignores, so the CV_7090 split collapses
    ::g_PacketManager.DisplayQuestArrow(state, (uint16_t)x, (uint16_t)y);

    g_AssistantEvents->OnTrack(state, x, y);
}

void CAssistant::HandleTargetType(uchar *buf)
{
    // Was g_TabListsTypes->SetTypeFromTarget(unpack16(buf + 17)); the field
    // update is the Phase 3 from-target flow (UI consumes via OnFromTarget).
    const ushort graphic = unpack16(buf + 17);
    g_AssistantEvents->OnFromTarget(XA_FROM_TARGET_TYPE, 0, graphic, 0, "");
}

void CAssistant::HandleTargetObject(uchar *buf)
{
    // Was g_TabListsObjects->SetObjectFromTarget(unpack32(buf + 7)).
    const uint serial = unpack32(buf + 7);
    if (serial != 0u)
    {
        Info(Assistant, "object from target: 0x%08X\n", serial);
    }
    g_AssistantEvents->OnFromTarget(XA_FROM_TARGET_OBJECT, serial, 0, 0, "");
}

void CAssistant::HandleTargetDisplayItem(uchar *buf)
{
    ushort color = 0;

    if (g_World != nullptr)
    {
        CGameObject *obj = g_World->FindWorldObject(unpack32(buf + 7));

        if (obj != nullptr)
        {
            color = obj->Color;
        }
    }

    // was g_TabDisplay->SetDisplayItemFromTarget(unpack16(buf + 17), color);
    // the items-editor graphic/hue fields are Phase 4 tab state
    g_AssistantEvents->OnFromTarget(
        XA_FROM_TARGET_DISPLAY_ITEM, 0, unpack16(buf + 17), color, "");
}

void CAssistant::HandleTargetInfo(uchar *buf)
{
    if (g_World != nullptr)
    {
        DumpObjectInfo(g_World->FindWorldObject(unpack32(buf + 7)));
    }
}

void CAssistant::HandleTargetInfoTile(uchar *buf)
{
    if (g_World != nullptr)
    {
        DumpTileInfo(unpack16(buf + 17), unpack16(buf + 11), unpack16(buf + 13), buf[16]);
    }
}

void CAssistant::HandleTargetDressBag(uchar *buf)
{
    SetDressBag(unpack32(buf + 7));
}

void CAssistant::HandleTargetDressItem(uchar *buf)
{
    if (g_World == nullptr)
    {
        return;
    }

    CGameItem *item = g_World->FindWorldItem(unpack32(buf + 7));

    if (item != nullptr)
    {
        int layer = EquipLayer(item);

        if ((layer != 0) && layer < OL_MOUNT && layer != OL_BACKPACK)
        {
            // was g_TabAgentsDress->AddDressItemInList(item); the working-set
            // edit is the Phase 4 tab (the tab re-resolves the item/layer
            // through the serial)
            g_AssistantEvents->OnFromTarget(
                XA_FROM_TARGET_DRESS_ITEM, item->Serial, item->Graphic,
                item->Color, "");
        }
    }
}

void CAssistant::HandleTargetHide(uchar *buf)
{
    ::g_PacketManager.DeleteObject(unpack32(buf + 7));
}

void CAssistant::HandleTargetDrop(uchar *buf)
{
    MoveItem(g_World->FindWorldItem(unpack32(buf + 7)));
}

void CAssistant::HandleTargetFindList(uchar *buf)
{
    // Was g_TabListsFind->AddFindListItemInList(g_World->FindWorldObject(...)).
    CGameObject *obj = g_World != nullptr ? g_World->FindWorldObject(unpack32(buf + 7)) : nullptr;
    if (obj != nullptr)
    {
        g_AssistantEvents->OnFromTarget(
            XA_FROM_TARGET_FIND_LIST, obj->Serial, obj->Graphic, obj->Color,
            obj->GetName());
    }
    // The original also clicked the object to fetch its name when unknown
    // (CObjectNameReceiver); the comment field just stays empty then.
}

void CAssistant::HandleTargetIgnoreList(uchar *buf)
{
    // Was g_TabListsIgnore->AddIgnoreListItemInList(...).
    CGameObject *obj = g_World != nullptr ? g_World->FindWorldObject(unpack32(buf + 7)) : nullptr;
    if (obj != nullptr)
    {
        g_AssistantEvents->OnFromTarget(
            XA_FROM_TARGET_IGNORE_LIST, obj->Serial, obj->Graphic, obj->Color,
            obj->GetName());
    }
}

void CAssistant::HandleTargetPartyObject(uchar *buf)
{
    // was g_TabAgentsParty->SetPartyObjectFromTarget(unpack32(buf + 7)); the
    // serial/name fields are Phase 4 tab state
    const uint serial = unpack32(buf + 7);
    astr_t name;

    if (g_World != nullptr)
    {
        CGameObject *obj = g_World->FindWorldObject(serial);

        if (obj != nullptr)
        {
            name = obj->GetName();
        }
    }

    g_AssistantEvents->OnFromTarget(XA_FROM_TARGET_PARTY, serial, 0, 0, name);
}

void CAssistant::HandleTargetFriendObject(uchar *buf)
{
    // Was g_TabListsFriends->SetFriendObjectFromTarget(unpack32(buf + 7)).
    const uint serial = unpack32(buf + 7);
    Info(Assistant, "friend from target: 0x%08X\n", serial);
    astr_t name;
    CGameObject *obj = g_World != nullptr ? g_World->FindWorldObject(serial) : nullptr;
    if (obj != nullptr)
    {
        name = obj->GetName();
    }
    g_AssistantEvents->OnFromTarget(XA_FROM_TARGET_FRIEND, serial, 0, 0, name);
}

void CAssistant::HandleTargetEnemyObject(uchar *buf)
{
    // Was g_TabListsEnemies->SetEnemyObjectFromTarget(unpack32(buf + 7)).
    const uint serial = unpack32(buf + 7);
    Info(Assistant, "enemy from target: 0x%08X\n", serial);
    astr_t name;
    CGameObject *obj = g_World != nullptr ? g_World->FindWorldObject(serial) : nullptr;
    if (obj != nullptr)
    {
        name = obj->GetName();
    }
    g_AssistantEvents->OnFromTarget(XA_FROM_TARGET_ENEMY, serial, 0, 0, name);
}

void CAssistant::HandleTargetAnimalAndVendorControlName(uchar *buf)
{
    // Was g_AnimalAndVendorControlForm->SetName(obj->GetName()) + window
    // activate; the control form is the Phase 3 speech popup.
    CGameObject *obj = g_World != nullptr ? g_World->FindWorldObject(unpack32(buf + 7)) : nullptr;
    if (obj != nullptr)
    {
        Info(Assistant, "animal/vendor name from target: %s\n", obj->GetName().c_str());
        g_AssistantEvents->OnFromTarget(
            XA_FROM_TARGET_ANIMAL_NAME, obj->Serial, obj->Graphic, obj->Color,
            obj->GetName());
    }
}

CGameObject *CAssistant::FindFriend(const ushort &flags, const astr_t &distanceText)
{
    int distance = g_AssistantConfig.DistanceFromText(distanceText);
    CGameObject *found = nullptr;
    CGameObject *start = g_World->m_Items;

    if (((flags & FTF_NEXT) != 0) && (g_LastFriendFound != 0u))
    {
        for (CGameObject *item = (CGameObject *)start; item != nullptr; item = (CGameObject *)item->m_Next)
        {
            if (item->Serial == g_LastFriendFound)
            {
                start = (CGameObject *)item->m_Next;
                break;
            }
        }
    }

    for (CGameObject *item = (CGameObject *)start; item != nullptr; item = (CGameObject *)item->m_Next)
    {
        if (item->Ignored || !item->NPC)
        {
            continue;
        }
        if (!g_AssistantConfig.InFriendList(item->Serial))
        {
            continue;
        }
        if (GetDistance(g_Player, item) > distance)
        {
            continue;
        }

        CGameCharacter *gc = (CGameCharacter *)item;

        bool dead = gc->IsDead();

        if (((flags & FTF_LIVE) != 0) && dead)
        {
            continue;
        }
        if (((flags & FTF_DEAD) != 0) && !dead)
        {
            continue;
        }
        if (((flags & FTF_INJURED) != 0) && ((gc->Hits == 0) || gc->Hits >= gc->MaxHits))
        {
            continue;
        }

        if ((flags & FTF_FAST) != 0)
        {
            return item;
        }
        if ((flags & FTF_NEAREST) != 0)
        {
            if (found == nullptr || GetDistance(g_Player, item) < GetDistance(g_Player, found))
            {
                found = item;
            }
        }
        else if (((flags & FTF_INJURED) != 0) && found != nullptr)
        {
            if (gc->Hits < ((CGameCharacter *)found)->Hits)
            {
                found = item;
            }
        }
        else
        {
            found = item;
        }
    }

    if (((flags & FTF_NEXT) != 0) && (g_LastFriendFound != 0u) && found == nullptr)
    {
        found = FindFriend(flags & (~FTF_NEXT), distanceText);
    }

    return found;
}

CGameObject *CAssistant::FindEnemy(const ushort &flags, const astr_t &distanceText)
{
    int distance = g_AssistantConfig.DistanceFromText(distanceText);
    CGameObject *found = nullptr;
    CGameObject *start = g_World->m_Items;

    if (((flags & FTF_NEXT) != 0) && (g_LastEnemyFound != 0u))
    {
        for (CGameObject *item = (CGameObject *)start; item != nullptr; item = (CGameObject *)item->m_Next)
        {
            if (item->Serial == g_LastEnemyFound)
            {
                start = (CGameObject *)item->m_Next;
                break;
            }
        }
    }

    for (CGameObject *item = (CGameObject *)g_World->m_Items; item != nullptr; item = (CGameObject *)item->m_Next)
    {
        if (item->Ignored || !item->NPC)
        {
            continue;
        }
        if (!g_AssistantConfig.InEnemyList(item->Serial))
        {
            continue;
        }
        if (GetDistance(g_Player, item) > distance)
        {
            continue;
        }

        CGameCharacter *gc = (CGameCharacter *)item;

        bool dead = gc->IsDead();

        if (((flags & FTF_LIVE) != 0) && dead)
        {
            continue;
        }
        if (((flags & FTF_DEAD) != 0) && !dead)
        {
            continue;
        }
        if (((flags & FTF_INJURED) != 0) && ((gc->Hits == 0) || gc->Hits >= gc->MaxHits))
        {
            continue;
        }

        if ((flags & FTF_FAST) != 0)
        {
            return item;
        }
        if ((flags & FTF_NEAREST) != 0)
        {
            if (found == nullptr || GetDistance(g_Player, item) < GetDistance(g_Player, found))
            {
                found = item;
            }
        }
        else if (((flags & FTF_INJURED) != 0) && found != nullptr)
        {
            if (gc->Hits < ((CGameCharacter *)found)->Hits)
            {
                found = item;
            }
        }
        else
        {
            found = item;
        }
    }

    if (((flags & FTF_NEXT) != 0) && (g_LastEnemyFound != 0u) && found == nullptr)
    {
        found = FindEnemy(flags & (~FTF_NEXT), distanceText);
    }

    return found;
}

void CAssistant::GetFriendsStatus()
{
    for (CGameObject *item = (CGameObject *)g_World->m_Items; item != nullptr; item = (CGameObject *)item->m_Next)
    {
        if (g_AssistantConfig.InFriendList(item->Serial))
        {
            GetStatus(item->Serial);
        }
    }
}

void CAssistant::GetEnemiesStatus()
{
    for (CGameObject *item = (CGameObject *)g_World->m_Items; item != nullptr; item = (CGameObject *)item->m_Next)
    {
        if (g_AssistantConfig.InEnemyList(item->Serial))
        {
            GetStatus(item->Serial);
        }
    }
}

bool CAssistant::IsNoDrawTile(const ushort &graphic)
{
    switch (graphic)
    {
        case 0x0001:
        case 0x21BC:
        case 0x9E4C:
        case 0x9E64:
        case 0x9E65:
        case 0x9E7D:
            return true;
        default:
            break;
    }

    if (graphic != 0x63D3)
    {
        if (graphic >= 0x2198 && graphic <= 0x21A4)
        {
            return true;
        }

        long long flags = g_GetStaticFlags(graphic);

        if (!IsNoDiagonal(flags) ||
            (IsAnimated(flags) && g_Player != nullptr && g_Player->Race == RT_GARGOYLE))
        {
            return false;
        }
    }

    return true;
}

void CAssistant::HandleSetPVPCaller(uchar *buf)
{
    g_SetInt(VKI_SET_PVPCALLER, unpack32(buf + 7));
}

void CAssistant::SetDressBag(const uint &serial)
{
    // Was TabListsObjects::SetDressBag (minus the label color updates; the
    // red/green dress-bag label is Phase 2 UI).
    if (g_World != nullptr && (serial != 0u))
    {
        CGameObject *obj = g_World->FindWorldObject(serial);

        if (obj != nullptr && !obj->NPC && (obj->Graphic != 0u))
        {
            if (!IsContainer(g_GetStaticFlags(obj->Graphic)))
            {
                ClientPrintDebugLevel1("This item is not a container!", 0x0021);
                return;
            }
        }
    }

    g_DressBag = serial;

    if (g_DressBag != 0u)
    {
        ClientPrintDebugLevel1("Dress bag set!", 0x0044);
    }
    else
    {
        ClientPrintDebugLevel1("Dress bag unset!", 0x0044);
    }
}

void CAssistant::OpenDoor()
{
    g_Game.OpenDoor();
}

void CAssistant::WalkAutoOpenDoor(const int &direction)
{
    // Was the front half of the 0x02 WalkRequest send tap (packetmanager.cpp):
    // probe the tile the walk steps onto (two tiles out when continuing
    // straight ahead) and open a door found there. The config gates the whole
    // step.
    if (g_Player == nullptr || !g_AssistantConfig.AutoOpenDoors() || g_Player->IsDead() ||
        g_Player->IsGM() || ((direction % 2) != 0))
    {
        return;
    }

    // dir is a plain value copy: a reference parameter named like the outer
    // direction local shadows it (gcc -Wshadow) for no benefit
    auto getNewXY = [](const int dir, int &x, int &y) {
        switch (dir & 7)
        {
            case 0:
                y--;
                break;
            case 1:
                x++;
                y--;
                break;
            case 2:
                x++;
                break;
            case 3:
                x++;
                y++;
                break;
            case 4:
                y++;
                break;
            case 5:
                x--;
                y++;
                break;
            case 6:
                x--;
                break;
            case 7:
                x--;
                y--;
                break;
            default:
                break;
        }
    };

    int x = g_Player->GetX();
    int y = g_Player->GetY();

    getNewXY(direction, x, y);

    if ((direction & 7) == (g_Player->Direction & 7))
    {
        getNewXY(direction, x, y);
    }

    AutoOpenDoor(x, y, g_Player->GetZ());
}

void CAssistant::AutoOpenDoor(const int &x, const int &y, const int &z)
{
    // Was AssistForm::AutoOpenDoor + onAutoOpenDoorTimer (assistform.cpp:
    // 780-822): find a door item at the target tile within +-10 z, then open
    // it (double click or the OpenDoor packet, per the config option). The
    // original timer was single-shot with an immediate-fire path; the immediate
    // path is the behavior here.
    uint doorToOpen = 0;

    if (g_World != nullptr)
    {
        int minZ = z - 10;
        int maxZ = z + 10;
        for (CGameObject *obj = (CGameObject *)g_World->m_Items; obj != nullptr; obj = (CGameObject *)obj->m_Next)
        {
            if (obj->NPC || !IsDoor(g_GetStaticFlags(obj->Graphic)) ||
                obj->GetX() != x || obj->GetY() != y)
            {
                continue;
            }

            int objZ = obj->GetZ();
            if (objZ < minZ || objZ > maxZ)
            {
                continue;
            }
            doorToOpen = obj->Serial;
            break;
        }
    }

    if (doorToOpen != 0u)
    {
        if (g_AssistantConfig.DoubleClickForOpenDoors())
        {
            DoubleClick(doorToOpen);
        }
        else
        {
            OpenDoor();
        }
    }
}

// The client's speech send sites (game console tail, MacroManager's say
// actions, CGame::SendASCIIText): were the 0x03 ClientTalkS / 0xAD
// UnicodeClientTalk send taps (the two rows differed only in text extraction
// from the wire - the 0xAD row decoded the encoded-command form to recover
// the typed text, which the send sites already hold). Runs the command-prefix
// intercept (returns false - the caller suppresses the send; the old taps
// returned "consumed" into a void entry point, so prefixed lines both ran
// the command and went out as raw speech, and the client-site early return
// restores the interception's purpose), records the say line and applies the
// font-color override to `color` by reference.
bool AssistantSpeechIntercept(astr_t &text, uint16_t &color)
{
    // Command prefix: run the assistant command and suppress the send.
    int prefixLen = g_AssistantConfig.IsCommandPrefix(text);
    if (prefixLen != 0)
    {
        g_TextCommandManager.DoCommand(text.substr(prefixLen));
        return false;
    }
    // DEVIATION: a leading '-' went to g_UOMapManager.UOAnotifyCmd
    // - the external UOA/UOAM command pipe (Windows WM_USER + atoms); there
    // is no in-process equivalent, the command stays a normal speech line.

    g_MacroExecutor.RecordSay(text);

    ushort replaceFontColorValue = 0;
    bool replaceFontColor = g_AssistantConfig.GetFontColor(replaceFontColorValue);

    if (!replaceFontColor && replaceFontColorValue != color)
    {
        g_AssistantConfig.SetFontColor(false, color);
    }

    if (replaceFontColor)
    {
        color = replaceFontColorValue;
    }

    return true;
}

} // namespace xuoassist
