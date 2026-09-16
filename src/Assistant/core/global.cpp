// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (global.cpp). The external-DLL host bridge pointers
// stay null until Bridge_Init (the first frame tick) wires them to client
// internals.

#include "global.h"
#include <cstring>
#include "CrossUO.h" // ::g_Game (ValueInt/GetStaticFlags bridge)

namespace xuoassist {

static uint64_t CDECL Bridge_GetStaticFlags(unsigned short graphic)
{
    return g_Game.GetStaticFlags(graphic);
}

static int CDECL Bridge_GetValueInt(VALUE_KEY_INT key, int value)
{
    return g_Game.ValueInt(key, value);
}

static void CDECL Bridge_SetValueInt(VALUE_KEY_INT key, int value)
{
    g_Game.ValueInt(key, value);
}

void Bridge_Init()
{
    static bool s_initialized = false;
    if (s_initialized)
    {
        return;
    }

    s_initialized = true;
    g_GetStaticFlags = Bridge_GetStaticFlags;
    g_GetInt = Bridge_GetValueInt;
    g_SetInt = Bridge_SetValueInt;
}

uint64_t g_EnabledFeature = XAEF_DEFAULT;
uint64_t g_EnabledCommands = XAEC_DEFAULT;

FUNCDEF_GET_STATIC_FLAGS *g_GetStaticFlags = nullptr;
FUNCDEF_GET_VALUE_INT *g_GetInt = nullptr;
FUNCDEF_SET_VALUE_INT *g_SetInt = nullptr;
uchar g_LightLevel = 0;
uchar g_PersonalLightLevel = 0;
SEASON_DATA g_Season = { 0, 0 };
WEATHER_DATA g_Weather = {
    0,
    0,
    0,
};
uint g_Backpack = 0;
bool g_UpdateDisplayOnStep = false;
uint g_DressBag = 0;
ushort g_MessagesColor = 1153;
ushort g_MorphGraphic = 0;
ushort g_OriginalGraphic = 0;
uint g_LastCorpseObject = 0;
uint g_LastContainerObject = 0;
uint g_LastUseObjectTimer = 0;
bool g_Connected = false;
uint g_LastFriendFound = 0;
uint g_LastEnemyFound = 0;
CPoint2Di g_LastTargetPosition(0, 0);
CPoint2Di g_LastAttackPosition(0, 0);
uint g_LastStatusRequest = 0;
uint g_LastStatusObject = 0;
uint g_LastUseObject = 0;
SelectedTile g_SelectedClientWorldObject = {};
CPoint2Di g_LastPlayerCorpsePosition = CPoint2Di(-1, -1);
uint g_LastPlayerDeadTimer = 0;
bool g_TargetReceived = false;
bool g_MenuReceived = false;
bool g_GumpReceived = false;
bool g_PromptReceived = false;
bool g_ContextMenuReceived = false;
bool g_TradeReceived = false;
bool g_ShopReceived = false;
uint g_LastTMapX = 0;
uint g_LastTMapY = 0;

} // namespace xuoassist
