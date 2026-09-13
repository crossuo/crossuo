// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (global.cpp). The external-DLL host bridge pointers
// stay null until the packet-manager round wires them to client internals;
// GetInt/GetString are null-safe for the same reason.

#include "global.h"
#include <cstring>
#include <common/str.h> // str_from (wide -> utf8)

namespace xuoassist {

int GetInt(VALUE_KEY_INT key, int value)
{
    if (g_GetInt == nullptr)
    {
        return value;
    }

    return g_GetInt(key, value);
}

string GetString(VALUE_KEY_STRING key, const string &value)
{
    if (g_GetString == nullptr)
    {
        return value;
    }

    IGameString *data = g_GetString(key, value.c_str());
    string result = "";

    if (data != nullptr)
    {
        if (data->m_Unicode)
        {
            // original: wide-char array converted to utf8
            result = str_from(std::wstring(data->m_DataW));
        }
        else
        {
            result = data->m_DataA;
        }
    }

    return result;
}

uint64_t g_EnabledFeature = XAEF_DEFAULT;
uint64_t g_EnabledCommands = XAEC_DEFAULT;

PACKET_PROC *g_SendClient = nullptr;
PACKET_PROC *g_SendServer = nullptr;
FUNCDEF_GET_STATIC_FLAGS *g_GetStaticFlags = nullptr;
FUNCDEF_GET_VALUE_INT *g_GetInt = nullptr;
FUNCDEF_SET_VALUE_INT *g_SetInt = nullptr;
FUNCDEF_GET_VALUE_STRING *g_GetString = nullptr;
FUNCDEF_SET_VALUE_STRING *g_SetString = nullptr;
uint g_LastSendTime = 0;
uint g_LastPacketTime = 0;
uint g_TotalSendSize = 0;
uint g_TotalRecvSize = 0;
uchar g_LightLevel = 0;
uchar g_PersonalLightLevel = 0;
SEASON_DATA g_Season = { 0, 0 };
WEATHER_DATA g_Weather = {
    0,
    0,
    0,
};
uint g_PlayerSerial = 0;
uint g_Backpack = 0;
uint g_FindItem = 0;
uint g_CurrentMap = 0;
string g_ServerName = "";
string g_CharacterName = "";
bool g_UpdateDisplayOnStep = false;
uint g_DressBag = 0;
ushort g_MessagesColor = 1153;
uint g_LastTargetObject = 0;
uint g_LastAttackObject = 0;
ushort g_MorphGraphic = 0;
ushort g_OriginalGraphic = 0;
uint g_LastCorpseObject = 0;
uint g_LastContainerObject = 0;
uint g_LastUseObjectTimer = 0;
uint g_StartTimer = 0;
bool g_Connected = false;
uint g_LastFriendFound = 0;
uint g_LastEnemyFound = 0;
uchar g_Ability[2] = { AT_DISARM, AT_PARALYZING_BLOW };
CPoint2Di g_LastTargetPosition(0, 0);
CPoint2Di g_LastAttackPosition(0, 0);
uint g_LastStatusRequest = 0;
uint g_LastStatusObject = 0;
uint g_LastUseObject = 0;
UOI_SELECTED_TILE g_SelectedClientWorldObject = {};
bool g_ClientMacroPlayed = false;
CPoint2Di g_LastPlayerCorpsePosition = CPoint2Di(-1, -1);
uint g_LastPlayerDeadTimer = 0;
int g_LastSkillIndex = 1;
int g_LastSpellIndex = 1;
bool g_TargetReceived = false;
bool g_MenuReceived = false;
bool g_GumpReceived = false;
bool g_PromptReceived = false;
bool g_ContextMenuReceived = false;
bool g_TradeReceived = false;
bool g_ShopReceived = false;
CPoint2Di g_RemoveRangeXY = CPoint2Di();
uint g_LastTMapX = 0;
uint g_LastTMapY = 0;

ushort g_SpellReagents[64][5] = {
    { 2, 0x0F7B, 0x0F88, 0, 0 },           { 3, 0x0F84, 0x0F85, 0x0F86, 0 },
    { 2, 0x0F85, 0x0F88, 0, 0 },           { 3, 0x0F84, 0x0F85, 0x0F8D, 0 },
    { 2, 0x0F7A, 0x0F88, 0, 0 },           { 2, 0x0F8C, 0x0F8D, 0, 0 },
    { 3, 0x0F84, 0x0F8C, 0x0F8D, 0 },      { 2, 0x0F84, 0x0F88, 0, 0 },
    { 2, 0x0F7B, 0x0F86, 0, 0 },           { 2, 0x0F86, 0x0F88, 0, 0 },
    { 2, 0x0F84, 0x0F85, 0, 0 },           { 2, 0x0F88, 0x0F8D, 0, 0 },
    { 3, 0x0F84, 0x0F8C, 0x0F8D, 0 },      { 2, 0x0F7B, 0x0F8C, 0, 0 },
    { 3, 0x0F84, 0x0F85, 0x0F8C, 0 },      { 2, 0x0F86, 0x0F88, 0, 0 },
    { 2, 0x0F84, 0x0F86, 0, 0 },           { 2, 0x0F7A, 0x0F8C, 0, 0 },
    { 3, 0x0F8C, 0x0F7B, 0x0F84, 0 },      { 1, 0x0F88, 0, 0, 0 },
    { 2, 0x0F7B, 0x0F86, 0, 0 },           { 2, 0x0F7B, 0x0F86, 0, 0 },
    { 2, 0x0F7B, 0x0F8C, 0, 0 },           { 2, 0x0F7B, 0x0F84, 0, 0 },
    { 3, 0x0F84, 0x0F85, 0x0F86, 0 },      { 4, 0x0F84, 0x0F85, 0x0F86, 0x0F8C },
    { 3, 0x0F84, 0x0F88, 0x0F8C, 0 },      { 3, 0x0F7A, 0x0F8D, 0x0F8C, 0 },
    { 4, 0x0F84, 0x0F85, 0x0F86, 0x0F8D }, { 3, 0x0F7A, 0x0F86, 0x0F8C, 0 },
    { 3, 0x0F7A, 0x0F86, 0x0F8D, 0 },      { 3, 0x0F7A, 0x0F7B, 0x0F86, 0 },
    { 3, 0x0F7A, 0x0F86, 0x0F88, 0 },      { 4, 0x0F84, 0x0F7A, 0x0F8D, 0x0F8C },
    { 3, 0x0F7B, 0x0F84, 0x0F88, 0 },      { 3, 0x0F84, 0x0F86, 0x0F8D, 0 },
    { 4, 0x0F7A, 0x0F86, 0x0F88, 0x0F8C }, { 3, 0x0F84, 0x0F86, 0x0F8D, 0 },
    { 3, 0x0F7A, 0x0F88, 0x0F8D, 0 },      { 3, 0x0F7B, 0x0F86, 0x0F8D, 0 },
    { 3, 0x0F84, 0x0F86, 0x0F8C, 0 },      { 2, 0x0F7A, 0x0F88, 0, 0 },
    { 3, 0x0F7A, 0x0F86, 0x0F8C, 0 },      { 2, 0x0F7B, 0x0F88, 0, 0 },
    { 3, 0x0F7A, 0x0F7B, 0x0F86, 0 },      { 4, 0x0F84, 0x0F86, 0x0F88, 0x0F8C },
    { 3, 0x0F7A, 0x0F85, 0x0F8D, 0 },      { 3, 0x0F7B, 0x0F8C, 0, 0 },
    { 4, 0x0F7A, 0x0F86, 0x0F7B, 0x0F8C }, { 4, 0x0F7A, 0x0F86, 0x0F8D, 0x0F8C },
    { 2, 0x0F8D, 0x0F8C, 0, 0 },           { 3, 0x0F7A, 0x0F86, 0x0F8C, 0 },
    { 4, 0x0F7A, 0x0F7B, 0x0F86, 0x0F8D }, { 4, 0x0F7A, 0x0F84, 0x0F86, 0x0F8C },
    { 4, 0x0F7B, 0x0F8D, 0x0F86, 0x0F8C }, { 3, 0x0F7B, 0x0F86, 0x0F8D, 0 },
    { 4, 0x0F7B, 0x0F85, 0x0F86, 0x0F8C }, { 4, 0x0F7A, 0x0F7B, 0x0F86, 0x0F88 },
    { 3, 0x0F7B, 0x0F84, 0x0F85, 0 },      { 3, 0x0F7B, 0x0F86, 0x0F8D, 0 },
    { 4, 0x0F7B, 0x0F86, 0x0F8D, 0x0F8C }, { 3, 0x0F7B, 0x0F86, 0x0F8D, 0 },
    { 4, 0x0F7B, 0x0F86, 0x0F8D, 0x0F8C }, { 3, 0x0F7B, 0x0F86, 0x0F8D, 0 }
};

} // namespace xuoassist
