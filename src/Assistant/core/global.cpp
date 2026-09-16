// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (global.cpp).

#include "global.h"
#include <cstring>

namespace xuoassist {

uint64_t g_EnabledFeature = XAEF_DEFAULT;
uint64_t g_EnabledCommands = XAEC_DEFAULT;

uint8_t g_LightLevel = 0;
uint8_t g_PersonalLightLevel = 0;
SEASON_DATA g_Season = { 0, 0 };
WEATHER_DATA g_Weather = {
    0,
    0,
    0,
};
uint32_t g_Backpack = 0;
bool g_UpdateDisplayOnStep = false;
uint32_t g_DressBag = 0;
uint16_t g_MessagesColor = 1153;
uint16_t g_MorphGraphic = 0;
uint16_t g_OriginalGraphic = 0;
uint32_t g_LastCorpseObject = 0;
uint32_t g_LastContainerObject = 0;
uint32_t g_LastUseObjectTimer = 0;
bool g_Connected = false;
uint32_t g_LastFriendFound = 0;
uint32_t g_LastEnemyFound = 0;
CPoint2Di g_LastTargetPosition(0, 0);
CPoint2Di g_LastAttackPosition(0, 0);
uint32_t g_LastStatusRequest = 0;
uint32_t g_LastStatusObject = 0;
uint32_t g_LastUseObject = 0;
SelectedTile g_SelectedClientWorldObject = {};
CPoint2Di g_LastPlayerCorpsePosition = CPoint2Di(-1, -1);
uint32_t g_LastPlayerDeadTimer = 0;
bool g_TargetReceived = false;
bool g_MenuReceived = false;
bool g_GumpReceived = false;
bool g_PromptReceived = false;
bool g_ContextMenuReceived = false;
bool g_TradeReceived = false;
bool g_ShopReceived = false;
uint32_t g_LastTMapX = 0;
uint32_t g_LastTMapY = 0;

} // namespace xuoassist
