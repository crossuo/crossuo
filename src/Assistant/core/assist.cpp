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
#include "../script/scriptgump.h" // the text window dump (the g_TextDialog port)
#include "packets.h"
#include "textconv.h"
#include "target.h"
#include "assistantevents.h"
#include "../model/gameobjects/gameworld.h"
#include "../model/gameobjects/gameplayer.h"
#include "textparser.h"
#include "../managers/packetmanager.h"
#include "../config/assistantconfig.h"
#include <common/str.h>
#include <common/logging/logging.h>
#include <stdlib.h>

namespace xuoassist {

static string to_lower_copy(string text)
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

void CAssistant::ScriptError(const string &title, const string &text)
{
    // Was CAssistant::slot_ScriptError.
    ClientPrint(title, 0x0021);
    ClientPrint(text, 0x0021);
}

void CAssistant::SendClient(uint8_t *buf, size_t size)
{
    g_SendClient(buf, size);
}

void CAssistant::SendServer(uint8_t *buf, size_t size)
{
    g_SendServer(buf, size);
}

uint CAssistant::RawStringToUInt(const string &value)
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

bool CAssistant::RawStringToBool(string value)
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

uint CAssistant::TextToUInt(const string &text)
{
    return ::xuoassist::TextToUInt(text); // core/textconv (shared with the model)
}

uint CAssistant::TextToSerial(string text)
{
    return ::xuoassist::TextToSerial(to_lower_copy(text));
}

ushort CAssistant::TextToGraphic(string text)
{
    return ::xuoassist::TextToGraphic(to_lower_copy(text));
}

int CAssistant::TextToLayer(string text)
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

string CAssistant::ColorToText(const uint &value)
{
    char buf[64] = { 0 };
    snprintf(buf, sizeof(buf), "#%08X", value);
    return buf;
}

string CAssistant::SerialToText(const uint &value)
{
    return ::xuoassist::SerialToText(value);
}

string CAssistant::GraphicToText(const ushort &value)
{
    return ::xuoassist::GraphicToText(value);
}

string CAssistant::BoolToText(const bool &value)
{
    return value ? "true" : "false";
}

void CAssistant::ClientPrint(const string &text, const ushort &color)
{
    CPacketToClientSystemPrint(wstr_from(text), color).SendClient();
}

void CAssistant::ClientCharPrint(const uint &serial, const string &text, const ushort &color)
{
    CPacketToClientCharacterPrint(serial, wstr_from(text), color).SendClient();
}

void CAssistant::ClientPrintDebugLevel1(const string &text, const ushort &color)
{
    if (g_AssistantConfig.GetMessagesLevel() >= 1)
    {
        ClientPrint(text, color);
    }
}

void CAssistant::ClientPrintDebugLevel2(const string &text, const ushort &color)
{
    if (g_AssistantConfig.GetMessagesLevel() >= 2)
    {
        ClientPrint(text, color);
    }
}

void CAssistant::RequestTarget(const string &text)
{
    if (text.length() != 0)
    {
        ClientPrintDebugLevel2(text);
    }

    g_Target.SetFromAssist(true);
    CPacketToClientTargetRequest().SendClient();
}

void CAssistant::DumpObjectInfo(CGameObject *obj)
{
    if (obj == nullptr)
    {
        return;
    }

    if (g_AssistantConfig.ObjectInspector())
    {
        g_AssistantEvents->OnInspectorUpdate(obj->GetSerial());
        return;
    }

    // the g_TextDialog->AddText + show/activateWindow body is restored by the
    // 4b-2 text window below; the same text goes through the OnJournalMessage
    // seam (the original app did both)
    int layer = 0;
    if (!obj->GetNPC())
    {
        layer = ((CGameItem *)obj)->GetLayer();
    }

    char text[512] = { 0 };
    snprintf(
        text,
        sizeof(text),
        "Serial=0x%08X Graphic=0x%04X Name=%s\nCount: %i  Color: 0x%04X  Layer: %i\nX=%i Y=%i Z=%i C=0x%08X F=0x%02X\n",
        obj->GetSerial(),
        obj->GetGraphic(),
        obj->GetName().c_str(),
        obj->GetCount(),
        obj->GetColor(),
        layer,
        obj->GetX(),
        obj->GetY(),
        obj->GetZ(),
        obj->GetContainer(),
        obj->GetFlags());

    CJournalMessage message(0xFFFFFFFF, g_MessagesColor, text);
    message.SetFlags(JMT_SYSTEM);
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
    message.SetFlags(JMT_SYSTEM);
    g_AssistantEvents->OnJournalMessage(message);

    // the g_TextDialog body port (see DumpObjectInfo)
    ScriptTextWindow_Command(TW_CLEAR, "", 0, 0, "");
    ScriptTextWindow_Command(TW_PRINT, text, 0, 0, "");
    ScriptTextWindow_Command(TW_OPEN, "", 0, 0, "");
}

void CAssistant::GetStatus(const uint &serial)
{
    g_LastStatusRequest = serial;
    CPacketStatusRequest(serial).SendServer();
}

void CAssistant::Click(const uint &serial)
{
    CPacketClickRequest(serial).SendServer();
}

void CAssistant::DoubleClick(const uint &serial)
{
    g_LastUseObject = serial;
    g_LastUseObjectTimer = g_Ticks;
    CPacketDoubleClickRequest(serial).SendServer();
}

void CAssistant::Attack(const uint &serial)
{
    CPacketAttackRequest(serial).SendServer();

    if (g_World != nullptr)
    {
        CGameCharacter *obj = g_World->FindWorldCharacter(serial);

        if (obj != nullptr && (obj->GetMaxHits() == 0))
        {
            CPacketStatusRequest(serial).SendServer();
        }
    }
}

uchar CAssistant::GetNotorietyFlags(const string &text)
{
    uchar flags = NTF_ALL;

    CTextParser parser(to_lower_copy(text), "| ", "", "");

    vector<string> list = parser.ReadTokens();

    for (const string &str : list)
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
    const string &findListName,
    const string &containerText,
    const string &distanceText,
    const ushort &flags,
    const string &notorietyText,
    bool recurseSearch,
    vector<string> &result)
{
    vector<CFindItem> findListItems;

    CTextParser findListNameParser(findListName, "|", "", "");

    vector<string> listNames = findListNameParser.ReadTokens();

    for (const string &str : listNames)
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
    CTextParser ignoreListNameParser(m_UsedIgnoreList, "|", "", "");

    listNames = ignoreListNameParser.ReadTokens();

    for (const string &str : listNames)
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
            result.push_back(CAssistant::SerialToText(found->GetSerial()));
        }
    }

    return found;
}

CGameObject *CAssistant::FindType(
    const vector<string> &graphic,
    const vector<string> &color,
    const string &containerText,
    const string &distanceText,
    const ushort &flags,
    const string &notorietyText,
    bool recurseSearch,
    vector<string> &result)
{
    result.clear();

    if (g_World == nullptr)
    {
        return nullptr;
    }

    vector<CIgnoreItem> ignoreListItems;
    CTextParser ignoreListNameParser(m_UsedIgnoreList, "|", "", "");

    vector<string> listNames = ignoreListNameParser.ReadTokens();

    for (const string &str : listNames)
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
            result.push_back(CAssistant::SerialToText(found->GetSerial()));
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
    vector<string> &list)
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

    QFOR(item, start, CGameObject *)
    {
        if (!item->GetIgnored() && (!checkDistance || GetDistance(g_Player, item) <= distance) &&
            findListItem.Found(item->GetGraphic(), item->GetColor()))
        {
            if (mobileOnly && !item->GetNPC())
            {
                continue;
            }
            if (item->GetNPC())
            {
                if (itemsOnly)
                {
                    continue;
                }
                if (humanOnly && !item->IsHuman())
                {
                    continue;
                }

                bool dead = ((CGameCharacter *)item)->Dead();
                if (liveOnly && dead)
                {
                    continue;
                }
                if (deadOnly && !dead)
                {
                    continue;
                }
                if (ignoreFriends && g_AssistantConfig.InFriendList(item->GetSerial()))
                {
                    continue;
                }
                if (ignoreEnemies && g_AssistantConfig.InEnemyList(item->GetSerial()))
                {
                    continue;
                }
            }

            bool ignored = false;

            if (!ignoreList.empty())
            {
                for (const CIgnoreItem &ii : ignoreList)
                {
                    if (ii.Found(item->GetSerial(), item->GetGraphic(), item->GetColor()))
                    {
                        ignored = true;
                        break;
                    }
                }
            }

            if (!ignored && (notoriety != 0u))
            {
                if (item->GetNPC())
                {
                    ignored = true;
                    uchar itemNoto = ((CGameCharacter *)item)->GetNotoriety();

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
                list.push_back(CAssistant::SerialToText(item->GetSerial()));

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
            (item->GetNPC() || ((CGameItem *)item)->GetLayer() != OL_BANK))
        {
            containers.push_back(item);
        }
    }

    if (((flags & FTF_FAST) == 0) || result == nullptr)
    {
        for (CGameObject *item : containers)
        {
            CGameObject *temp = item->FindType(findListItem, ignoreList, flags, recurseSearch, list);

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
    const vector<string> &graphic,
    const vector<string> &color,
    const string &containerText,
    const string &distanceText,
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

    QFOR(item, start, CGameObject *)
    {
        if (findListItem.Found(item->GetGraphic(), item->GetColor()) &&
            (!checkDistance || GetDistance(g_Player, item) <= distance))
        {
            count += item->GetCount();
        }

        if (recurseSearch && !item->Empty() &&
            (item->GetNPC() || ((CGameItem *)item)->GetLayer() != OL_BANK))
        {
            count += item->CountItems(findListItem, recurseSearch);
        }
    }

    return count;
}

void CAssistant::UseType(
    const CFindItem &findListItem, const uint &container, const int &distance, const bool &recurseSearch)
{
    vector<string> list;

    CGameObject *item =
        FindType(findListItem, vector<CIgnoreItem>(), container, distance, FTF_NORMAL, 0, recurseSearch, list);

    if (item != nullptr)
    {
        DoubleClick(item->GetSerial());
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

    uint serial = item->GetSerial();

    if (g_MoveItemData.Count < 1)
    {
        g_MoveItemData.Count = item->GetCount();
    }

    CPacketPickupRequest(serial, g_MoveItemData.Count).SendServer();

    if (g_PacketManager.GetClientVersion() >= CV_6017)
    {
        CPacketDropRequestNew(
            serial, g_MoveItemData.X, g_MoveItemData.Y, g_MoveItemData.Z, 0, g_MoveItemData.Container)
            .SendServer();
    }
    else
    {
        CPacketDropRequestOld(
            serial, g_MoveItemData.X, g_MoveItemData.Y, g_MoveItemData.Z, g_MoveItemData.Container)
            .SendServer();
    }
}

void CAssistant::MoveEquip(const uint &serial, const int &layer, const uint &container)
{
    CPacketPickupRequest(serial, 1).SendServer();
    CPacketEquipRequest(serial, layer, container).SendServer();
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

    if (g_PacketManager.GetClientVersion() >= CV_7090)
    {
        CPacketQuestArrowNew(state, x, y).SendClient();
    }
    else
    {
        CPacketQuestArrowOld(state, x, y).SendClient();
    }

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
            color = obj->GetColor();
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
        int layer = item->GetEquipLayer();

        if ((layer != 0) && layer < OL_MOUNT && layer != OL_BACKPACK)
        {
            // was g_TabAgentsDress->AddDressItemInList(item); the working-set
            // edit is the Phase 4 tab (the tab re-resolves the item/layer
            // through the serial)
            g_AssistantEvents->OnFromTarget(
                XA_FROM_TARGET_DRESS_ITEM, item->GetSerial(), item->GetGraphic(),
                item->GetColor(), "");
        }
    }
}

void CAssistant::HandleTargetHide(uchar *buf)
{
    CPacketHideObject(unpack32(buf + 7)).SendClient();
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
            XA_FROM_TARGET_FIND_LIST, obj->GetSerial(), obj->GetGraphic(), obj->GetColor(),
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
            XA_FROM_TARGET_IGNORE_LIST, obj->GetSerial(), obj->GetGraphic(), obj->GetColor(),
            obj->GetName());
    }
}

void CAssistant::HandleTargetPartyObject(uchar *buf)
{
    // was g_TabAgentsParty->SetPartyObjectFromTarget(unpack32(buf + 7)); the
    // serial/name fields are Phase 4 tab state
    const uint serial = unpack32(buf + 7);
    string name;

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
    string name;
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
    string name;
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
            XA_FROM_TARGET_ANIMAL_NAME, obj->GetSerial(), obj->GetGraphic(), obj->GetColor(),
            obj->GetName());
    }
}

CGameObject *CAssistant::FindFriend(const ushort &flags, const string &distanceText)
{
    int distance = g_AssistantConfig.DistanceFromText(distanceText);
    CGameObject *found = nullptr;
    CGameObject *start = g_World->m_Items;

    if (((flags & FTF_NEXT) != 0) && (g_LastFriendFound != 0u))
    {
        QFOR(item, start, CGameObject *)
        {
            if (item->GetSerial() == g_LastFriendFound)
            {
                start = (CGameObject *)item->m_Next;
                break;
            }
        }
    }

    QFOR(item, start, CGameObject *)
    {
        if (item->GetIgnored() || !item->GetNPC())
        {
            continue;
        }
        if (!g_AssistantConfig.InFriendList(item->GetSerial()))
        {
            continue;
        }
        if (GetDistance(g_Player, item) > distance)
        {
            continue;
        }

        CGameCharacter *gc = (CGameCharacter *)item;

        bool dead = gc->Dead();

        if (((flags & FTF_LIVE) != 0) && dead)
        {
            continue;
        }
        if (((flags & FTF_DEAD) != 0) && !dead)
        {
            continue;
        }
        if (((flags & FTF_INJURED) != 0) && ((gc->GetHits() == 0) || gc->GetHits() >= gc->GetMaxHits()))
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
            if (gc->GetHits() < ((CGameCharacter *)found)->GetHits())
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

CGameObject *CAssistant::FindEnemy(const ushort &flags, const string &distanceText)
{
    int distance = g_AssistantConfig.DistanceFromText(distanceText);
    CGameObject *found = nullptr;
    CGameObject *start = g_World->m_Items;

    if (((flags & FTF_NEXT) != 0) && (g_LastEnemyFound != 0u))
    {
        QFOR(item, start, CGameObject *)
        {
            if (item->GetSerial() == g_LastEnemyFound)
            {
                start = (CGameObject *)item->m_Next;
                break;
            }
        }
    }

    QFOR(item, g_World->m_Items, CGameObject *)
    {
        if (item->GetIgnored() || !item->GetNPC())
        {
            continue;
        }
        if (!g_AssistantConfig.InEnemyList(item->GetSerial()))
        {
            continue;
        }
        if (GetDistance(g_Player, item) > distance)
        {
            continue;
        }

        CGameCharacter *gc = (CGameCharacter *)item;

        bool dead = gc->Dead();

        if (((flags & FTF_LIVE) != 0) && dead)
        {
            continue;
        }
        if (((flags & FTF_DEAD) != 0) && !dead)
        {
            continue;
        }
        if (((flags & FTF_INJURED) != 0) && ((gc->GetHits() == 0) || gc->GetHits() >= gc->GetMaxHits()))
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
            if (gc->GetHits() < ((CGameCharacter *)found)->GetHits())
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
    QFOR(item, g_World->m_Items, CGameObject *)
    {
        if (g_AssistantConfig.InFriendList(item->GetSerial()))
        {
            GetStatus(item->GetSerial());
        }
    }
}

void CAssistant::GetEnemiesStatus()
{
    QFOR(item, g_World->m_Items, CGameObject *)
    {
        if (g_AssistantConfig.InEnemyList(item->GetSerial()))
        {
            GetStatus(item->GetSerial());
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
            (IsAnimated(flags) && g_Player != nullptr && g_Player->GetRace() == RT_GARGOYLE))
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

        if (obj != nullptr && !obj->GetNPC() && (obj->GetGraphic() != 0u))
        {
            if (!IsContainer(g_GetStaticFlags(obj->GetGraphic())))
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
    CPacketOpenDoorRequest().SendClient();
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
        QFOR(obj, g_World->m_Items, CGameObject *)
        {
            if (obj->GetNPC() || !IsDoor(((CGameItem *)obj)->GetTiledataFlags()) ||
                obj->GetX() != x || obj->GetY() != y)
            {
                continue;
            }

            int objZ = obj->GetZ();
            if (objZ < minZ || objZ > maxZ)
            {
                continue;
            }
            doorToOpen = obj->GetSerial();
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

} // namespace xuoassist
