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
typedef void (CAssistant::*TARGET_HANDLER)(uchar *buf);
extern TARGET_HANDLER g_TargetHandler;

struct MOVITEM_DATA
{
    int Count{ 0 };
    uint Container{ 0xFFFFFFFF };
    ushort X{ 0xFFFF };
    ushort Y{ 0xFFFF };
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

    static uint RawStringToUInt(const astr_t &value);
    static bool RawStringToBool(astr_t value);
    static uint TextToUInt(const astr_t &text);
    static uint TextToSerial(astr_t text);
    static ushort TextToGraphic(astr_t text);
    static int TextToLayer(astr_t text);
    static astr_t ColorToText(const uint &value);
    static astr_t SerialToText(const uint &value);
    static astr_t GraphicToText(const ushort &value);
    static astr_t BoolToText(const bool &value);

    void ClientPrint(const astr_t &text, const ushort &color = g_MessagesColor);
    void ClientCharPrint(const uint &serial, const astr_t &text, const ushort &color = g_MessagesColor);
    void ClientPrintDebugLevel1(const astr_t &text, const ushort &color = g_MessagesColor);
    void ClientPrintDebugLevel2(const astr_t &text, const ushort &color = g_MessagesColor);

    void RequestTarget(const astr_t &text);

    // the client 0xD1 handler performs the logout request (CGame::LogOut,
    // dispatched via the DIR_RECV-registered HandleLogout, so the old injected
    // 0xD1 was not a no-op); the assistant now calls it directly
    void LogOut();

    void DumpObjectInfo(class CGameObject *obj);
    void DumpTileInfo(const ushort &graphic, const short &x, const short &y, const char &z);

    void GetStatus(const uint &serial);
    void Click(const uint &serial);
    void DoubleClick(const uint &serial);
    void Attack(const uint &serial);
    uchar GetNotorietyFlags(const astr_t &text);

    class CGameObject *FindList(
        const astr_t &findListName,
        const astr_t &containerText,
        const astr_t &distanceText,
        const ushort &flags,
        const astr_t &notorietyText,
        bool recurseSearch,
        vector<astr_t> &result);
    class CGameObject *FindType(
        const vector<astr_t> &graphic,
        const vector<astr_t> &color,
        const astr_t &containerText,
        const astr_t &distanceText,
        const ushort &flags,
        const astr_t &notorietyText,
        bool recurseSearch,
        vector<astr_t> &result);
    class CGameObject *FindType(
        const CFindItem &findListItem,
        const vector<CIgnoreItem> &ignoreList,
        const uint &container,
        const int &distance,
        ushort flags,
        const uchar &notoriety,
        bool recurseSearch,
        vector<astr_t> &list);
    class CGameObject *FindFriend(const ushort &flags, const astr_t &distanceText);
    class CGameObject *FindEnemy(const ushort &flags, const astr_t &distanceText);
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
        const uint &container,
        const int &distance,
        bool recurseSearch);
    void UseType(
        const CFindItem &findListItem,
        const uint &container,
        const int &distance,
        const bool &recurseSearch);
    void MoveItem(class CGameItem *item);
    void MoveEquip(const uint &serial, const int &layer, const uint &container);
    void Track(const bool &state, short x, short y);

    // Moved from the original TabListsObjects::SetDressBag (the dress bag is config
    // data; the container check + assistant messages were its logic).
    void SetDressBag(const uint &serial);

    // Moved from the original AssistForm::AutoOpenDoor + onAutoOpenDoorTimer (the
    // single-shot 20 ms timer collapsed - the scan and the open are one step
    // now, exactly like the timer's immediate-fire path).
    void AutoOpenDoor(const int &x, const int &y, const int &z);

    // The auto-open-door half of the walk send (was the front half of the 0x02
    // WalkRequest send tap, packetmanager.cpp): called from the client's
    // CPathFinder::Walk with the packet's direction byte.
    void WalkAutoOpenDoor(const int &direction);

    void HandleTargetType(uchar *buf);
    void HandleTargetObject(uchar *buf);
    void HandleTargetDisplayItem(uchar *buf);
    void HandleTargetInfo(uchar *buf);
    void HandleTargetInfoTile(uchar *buf);
    void HandleTargetDressBag(uchar *buf);
    void HandleTargetDressItem(uchar *buf);
    void HandleTargetHide(uchar *buf);
    void HandleTargetDrop(uchar *buf);
    void HandleTargetFindList(uchar *buf);
    void HandleTargetIgnoreList(uchar *buf);
    void HandleTargetPartyObject(uchar *buf);
    void HandleTargetFriendObject(uchar *buf);
    void HandleTargetEnemyObject(uchar *buf);
    void HandleTargetAnimalAndVendorControlName(uchar *buf);
    void HandleSetPVPCaller(uchar *buf);
    void OpenDoor();

    static bool IsNoDrawTile(const ushort &graphic);

    void ScriptError(const astr_t &title, const astr_t &text);
};

extern CAssistant g_Assistant;

// The client's speech send sites (game console tail, MacroManager's say
// actions, CGame::SendASCIIText): runs the command-prefix intercept (returns
// false when the caller must suppress the send), records the say line and
// applies the font-color override to `color` by reference.
bool AssistantSpeechIntercept(astr_t &text, uint16_t &color);

} // namespace xuoassist
