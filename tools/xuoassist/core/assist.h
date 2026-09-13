// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (assist/assist.h), trimmed per assistant-plan.md
// §4.1: the base object, the cross-thread signal pair (signal_SendClient/
// signal_SendServer -> direct g_SendClient/g_SendServer calls) and the
// parsing helpers are dropped; FIND_TYPE_FLAGS /
// NOTORIETY_TYPE_FLAGS moved to core/global.h (shared with the model layer).
// The data-list lookups that used to poke g_TabLists* widget globals consult
// the AssistantConfig module instead (same module that loads/saves them).

#pragma once

#include "global.h"
#include "../model/commonitems/skill.h"
#include "../model/commonitems/finditem.h"
#include "../model/commonitems/ignoreitem.h"

namespace xuoassist {

class CGameObject;
class CGameItem;

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
    string m_UsedIgnoreList;

public:
    // SETGET(UsedIgnoreList) in the original
    string GetUsedIgnoreList() const { return m_UsedIgnoreList; }
    void SetUsedIgnoreList(const string &val) { m_UsedIgnoreList = val; }

    CAssistant();
    ~CAssistant();

    void Init() {}

    static uint RawStringToUInt(const string &value);
    static bool RawStringToBool(string value);
    static uint TextToUInt(const string &text);
    static uint TextToSerial(string text);
    static ushort TextToGraphic(string text);
    static int TextToLayer(string text);
    static string ColorToText(const uint &value);
    static string SerialToText(const uint &value);
    static string GraphicToText(const ushort &value);
    static string BoolToText(const bool &value);

    void SendClient(uint8_t *buf, size_t size);
    void SendServer(uint8_t *buf, size_t size);

    void SendClient(vector<uint8_t> buf) { SendClient(&buf[0], buf.size()); }
    void SendServer(vector<uint8_t> buf) { SendServer(&buf[0], buf.size()); }

    void ClientPrint(const string &text, const ushort &color = g_MessagesColor);
    void ClientCharPrint(const uint &serial, const string &text, const ushort &color = g_MessagesColor);
    void ClientPrintDebugLevel1(const string &text, const ushort &color = g_MessagesColor);
    void ClientPrintDebugLevel2(const string &text, const ushort &color = g_MessagesColor);

    void RequestTarget(const string &text);

    void DumpObjectInfo(class CGameObject *obj);
    void DumpTileInfo(const ushort &graphic, const short &x, const short &y, const char &z);

    void GetStatus(const uint &serial);
    void Click(const uint &serial);
    void DoubleClick(const uint &serial);
    void Attack(const uint &serial);
    uchar GetNotorietyFlags(const string &text);

    class CGameObject *FindList(
        const string &findListName,
        const string &containerText,
        const string &distanceText,
        const ushort &flags,
        const string &notorietyText,
        bool recurseSearch,
        vector<string> &result);
    class CGameObject *FindType(
        const vector<string> &graphic,
        const vector<string> &color,
        const string &containerText,
        const string &distanceText,
        const ushort &flags,
        const string &notorietyText,
        bool recurseSearch,
        vector<string> &result);
    class CGameObject *FindType(
        const CFindItem &findListItem,
        const vector<CIgnoreItem> &ignoreList,
        const uint &container,
        const int &distance,
        ushort flags,
        const uchar &notoriety,
        bool recurseSearch,
        vector<string> &list);
    class CGameObject *FindFriend(const ushort &flags, const string &distanceText);
    class CGameObject *FindEnemy(const ushort &flags, const string &distanceText);
    void GetFriendsStatus();
    void GetEnemiesStatus();
    int CountType(
        const vector<string> &graphic,
        const vector<string> &color,
        const string &containerText,
        const string &distanceText,
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

    void ScriptError(const string &title, const string &text);
};

extern CAssistant g_Assistant;

} // namespace xuoassist

