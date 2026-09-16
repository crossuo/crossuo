// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Core assistant object: journal, speech filters, find/count/ignore
// engine and the client/server send helpers. FIND_TYPE_FLAGS /
// NOTORIETY_TYPE_FLAGS live in core/global.h.
// The data-list lookups that used to poke g_TabLists* widget globals consult
// the AssistantConfig module instead (same module that loads/saves them).

#pragma once

#include "global.h"
#include "../model/commonitems/finditem.h"
#include "../model/commonitems/ignoreitem.h"

// the client's world objects (the assistant has no world model of its own)
class CGameObject;
class CGameItem;
class CPlayer;

namespace xuoassist {


class CAssistant;
typedef void (CAssistant::*TARGET_HANDLER)(uint8_t *buf);
extern TARGET_HANDLER g_TargetHandler;

struct MOVITEM_DATA
{
    int Count{ 0 };
    uint32_t Container{ 0xFFFFFFFF };
    uint16_t X{ 0xFFFF };
    uint16_t Y{ 0xFFFF };
    char Z{ 0 };

    void Reset()
    {
        Count = 0;
        Container = 0xFFFFFFFF;
        X = 0xFFFF;
        Y = 0xFFFF;
        Z = 0;
    }
};

extern MOVITEM_DATA g_MoveItemData;

class CAssistant
{
    astr_t m_UsedIgnoreList;

public:
    astr_t GetUsedIgnoreList() const { return m_UsedIgnoreList; }
    void SetUsedIgnoreList(const astr_t &val) { m_UsedIgnoreList = val; }

    CAssistant();
    ~CAssistant();

    void Init() {}

    static uint32_t RawStringToUInt(const astr_t &value);
    static bool RawStringToBool(astr_t value);
    static uint32_t TextToUInt(const astr_t &text);
    static uint32_t TextToSerial(astr_t text);
    static uint16_t TextToGraphic(astr_t text);
    static int TextToLayer(astr_t text);
    static astr_t ColorToText(const uint32_t &value);
    static astr_t SerialToText(const uint32_t &value);
    static astr_t GraphicToText(const uint16_t &value);
    static astr_t BoolToText(const bool &value);

    void ClientPrint(const astr_t &text, const uint16_t &color = g_MessagesColor);
    void ClientCharPrint(const uint32_t &serial, const astr_t &text, const uint16_t &color = g_MessagesColor);
    void ClientPrintDebugLevel1(const astr_t &text, const uint16_t &color = g_MessagesColor);
    void ClientPrintDebugLevel2(const astr_t &text, const uint16_t &color = g_MessagesColor);

    void RequestTarget(const astr_t &text);

    // the client 0xD1 handler performs the logout request (CGame::LogOut,
    // dispatched via the DIR_RECV-registered HandleLogout, so the old injected
    // 0xD1 was not a no-op); the assistant now calls it directly
    void LogOut();

    void DumpObjectInfo(class CGameObject *obj);
    void DumpTileInfo(const uint16_t &graphic, const short &x, const short &y, const char &z);

    void GetStatus(const uint32_t &serial);
    void Click(const uint32_t &serial);
    void DoubleClick(const uint32_t &serial);
    void Attack(const uint32_t &serial);
    uint8_t GetNotorietyFlags(const astr_t &text);

    class CGameObject *FindList(
        const astr_t &findListName,
        const astr_t &containerText,
        const astr_t &distanceText,
        const uint16_t &flags,
        const astr_t &notorietyText,
        bool recurseSearch,
        vector<astr_t> &result);
    class CGameObject *FindType(
        const vector<astr_t> &graphic,
        const vector<astr_t> &color,
        const astr_t &containerText,
        const astr_t &distanceText,
        const uint16_t &flags,
        const astr_t &notorietyText,
        bool recurseSearch,
        vector<astr_t> &result);
    class CGameObject *FindType(
        const CFindItem &findListItem,
        const vector<CIgnoreItem> &ignoreList,
        const uint32_t &container,
        const int &distance,
        uint16_t flags,
        const uint8_t &notoriety,
        bool recurseSearch,
        vector<astr_t> &list);
    class CGameObject *FindFriend(const uint16_t &flags, const astr_t &distanceText);
    class CGameObject *FindEnemy(const uint16_t &flags, const astr_t &distanceText);
    void GetFriendsStatus();
    void GetEnemiesStatus();
    int CountType(
        const vector<astr_t> &graphic,
        const vector<astr_t> &color,
        const astr_t &containerText,
        const astr_t &distanceText,
        bool recurseSearch);
    int CountType(
        const CFindItem &findListItem,
        const uint32_t &container,
        const int &distance,
        bool recurseSearch);
    void UseType(
        const CFindItem &findListItem,
        const uint32_t &container,
        const int &distance,
        const bool &recurseSearch);
    void MoveItem(class CGameItem *item);
    void MoveEquip(const uint32_t &serial, const int &layer, const uint32_t &container);
    void Track(const bool &state, short x, short y);

    // Moved from the original TabListsObjects::SetDressBag (the dress bag is config
    // data; the container check + assistant messages were its logic).
    void SetDressBag(const uint32_t &serial);

    // Moved from the original AssistForm::AutoOpenDoor + onAutoOpenDoorTimer (the
    // single-shot 20 ms timer collapsed - the scan and the open are one step
    // now, exactly like the timer's immediate-fire path).
    void AutoOpenDoor(const int &x, const int &y, const int &z);

    // The auto-open-door half of the walk send (was the front half of the 0x02
    // WalkRequest send tap, packetmanager.cpp): called from the client's
    // CPathFinder::Walk with the packet's direction byte.
    void WalkAutoOpenDoor(const int &direction);

    void HandleTargetType(uint8_t *buf);
    void HandleTargetObject(uint8_t *buf);
    void HandleTargetDisplayItem(uint8_t *buf);
    void HandleTargetInfo(uint8_t *buf);
    void HandleTargetInfoTile(uint8_t *buf);
    void HandleTargetDressBag(uint8_t *buf);
    void HandleTargetDressItem(uint8_t *buf);
    void HandleTargetHide(uint8_t *buf);
    void HandleTargetDrop(uint8_t *buf);
    void HandleTargetFindList(uint8_t *buf);
    void HandleTargetIgnoreList(uint8_t *buf);
    void HandleTargetPartyObject(uint8_t *buf);
    void HandleTargetFriendObject(uint8_t *buf);
    void HandleTargetEnemyObject(uint8_t *buf);
    void HandleTargetAnimalAndVendorControlName(uint8_t *buf);
    void HandleSetPVPCaller(uint8_t *buf);
    void OpenDoor();

    static bool IsNoDrawTile(const uint16_t &graphic);

    void ScriptError(const astr_t &title, const astr_t &text);
};

extern CAssistant g_Assistant;

// The client's speech send sites (game console tail, MacroManager's say
// actions, CGame::SendASCIIText): runs the command-prefix intercept (returns
// false when the caller must suppress the send), records the say line and
// applies the font-color override to `color` by reference.
bool AssistantSpeechIntercept(astr_t &text, uint16_t &color);

} // namespace xuoassist
