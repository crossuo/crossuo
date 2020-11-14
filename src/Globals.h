// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <stdint.h>
#include "GLEngine/GLTexture.h"     // REMOVE
#include "GLEngine/GLFrameBuffer.h" // REMOVE
#include <common/str.h>             // REMOVE
#include "Point.h"
#include "Constants.h"
#include <xuocore/enumlist.h>

#define IS_MOBILE(serial) ((serial & 0x40000000) == 0)
#define IS_ITEM(serial) ((serial & 0x40000000) != 0)
#define IS_PLAYER(serial) (serial == g_PlayerSerial)

#define ToColorR(x) ((x)&0xff)
#define ToColorG(x) ((x >> 8) & 0xff)
#define ToColorB(x) ((x >> 16) & 0xff)
#define ToColorA(x) ((x >> 24) & 0xff)

struct DebugContext
{
    uint16_t lastGumpId = 0;
    uint16_t lastGumpPartId = 0;
    uint16_t lastLandArtId = 0;
    uint16_t lastAnimId = 0;
    uint16_t lastStaticId = 0;
    uint16_t lastTextureId = 0;
    uint8_t lastLightId = 0;
};
extern DebugContext g_DebugContext;

enum
{
    WINDOW_INACTIVE = 0,
    WINDOW_ACTIVE = 1,
};

extern bool g_AltPressed;
extern bool g_CtrlPressed;
extern bool g_CmdPressed;
extern bool g_ShiftPressed;
extern bool g_MovingFromMouse;
extern bool g_AutoMoving;
extern bool g_AbyssPacket03First;
extern astr_t g_dumpUopFile;

bool CanBeDraggedByOffset(const CPoint2Di &point);
void TileOffsetOnMonitorToXY(int &ofsX, int &ofsY, int &x, int &y);
astr_t ToCamelCase(astr_t text);

class CGameObject;
int GetDistance(CGameObject *current, CGameObject *target);
int GetDistance(CGameObject *current, const CPoint2Di &target);
bool CheckMultiDistance(const CPoint2Di &current, CGameObject *target, int maxDistance);
int GetDistance(const CPoint2Di &current, CGameObject *target);
int GetDistance(const CPoint2Di &current, const CPoint2Di &target);
int GetRemoveDistance(const CPoint2Di &current, CGameObject *target);
int GetTopObjDistance(CGameObject *current, CGameObject *target);

extern int g_LandObjectsCount;
extern int g_StaticsObjectsCount;
extern int g_GameObjectsCount;
extern int g_MultiObjectsCount;
extern int g_RenderedObjectsCountInGameWindow;

extern float g_GlobalScale;

extern CGLTexture g_MapTexture[MAX_MAPS_COUNT]; // FIXME
extern CGLTexture g_AuraTexture;                // FIXME
extern CGLTexture g_TextureGumpState[2];        // FIXME
extern class CGLFrameBuffer g_LightBuffer;      // FIXME

extern bool g_LogoutAfterClick;

extern int g_FrameDelay[2];

extern uint32_t g_LastSendTime;

extern uint32_t g_LastPacketTime;

extern uint32_t g_TotalSendSize;

extern uint32_t g_TotalRecvSize;

extern uint32_t g_Ticks;

extern uint32_t g_ShaderColorTableInUse;
extern uint32_t g_ShaderDrawMode;

extern astr_t g_Language;

extern GAME_STATE g_GameState;

extern bool g_GumpPressed;
extern class CRenderObject *g_GumpSelectedElement;
extern class CRenderObject *g_GumpPressedElement;
extern CPoint2Di g_GumpMovingOffset;
extern CPoint2Df g_GumpTranslate;
extern bool g_ShowGumpLocker;

extern bool g_GrayedPixels;

extern bool g_ConfigLoaded;

extern uint8_t g_LightLevel;
extern uint8_t g_PersonalLightLevel;

extern char g_SelectedCharName[30];

extern uint8_t g_CurrentMap;

extern uint8_t g_ServerTimeHour;
extern uint8_t g_ServerTimeMinute;
extern uint8_t g_ServerTimeSecond;

extern bool g_PacketLoginComplete;

extern uint32_t g_ClientFlag;

extern bool g_SendLogoutNotification;
extern bool g_PopupEnabled;
extern bool g_ChatEnabled;
extern bool g_Pal_Necro_Creation;
extern bool g_TooltipsEnabled;
extern bool g_PaperdollBooks;

extern uint8_t g_GameSeed[4];

extern uint16_t g_OutOfRangeColor;

extern bool g_NoDrawRoof;

extern char g_MaxGroundZ;

extern char g_FoliageIndex;

extern bool g_UseCircleTrans;

extern bool g_JournalShowSystem;

extern bool g_JournalShowObjects;

extern bool g_JournalShowClient;

extern uint32_t g_PlayerSerial;
extern uint32_t g_StatusbarUnderMouse;

extern int g_LastSpellIndex;

extern int g_LastSkillIndex;

extern uint32_t g_LastUseObject;

extern uint32_t g_LastTargetObject;

extern uint32_t g_LastAttackObject;

extern CHARACTER_SPEED_TYPE g_SpeedMode;

extern uint32_t g_DeathScreenTimer;

extern float g_AnimCharactersDelayValue;

extern CPoint2Di g_RemoveRangeXY;

extern int g_GrayMenuCount;

extern PROMPT_TYPE g_ConsolePrompt;

extern uint8_t g_LastASCIIPrompt[11];

extern uint8_t g_LastUnicodePrompt[11];

extern uint32_t g_PartyHelperTarget;

extern uint32_t g_PartyHelperTimer;

extern float g_DrawColor;

extern SEASON_TYPE g_Season;
extern SEASON_TYPE g_OldSeason;
extern int g_OldSeasonMusic;

extern uint32_t g_LockedClientFeatures;

extern bool g_GeneratedMouseDown;

enum DEVELOPER_MODE
{
    DM_NO_DEBUG = 0,
    DM_SHOW_FPS_ONLY,
    DM_DEBUGGING
};
extern DEVELOPER_MODE g_DeveloperMode;
extern DEVELOPER_MODE g_OptionsDeveloperMode;

const int g_ObjectHandlesWidth = 142;
const int g_ObjectHandlesWidthOffset = g_ObjectHandlesWidth / 2;
const int g_ObjectHandlesHeight = 20;
extern uint16_t g_ObjectHandlesBackgroundPixels[g_ObjectHandlesWidth * g_ObjectHandlesHeight];

extern bool g_DrawAura;

static const int MAX_ABILITIES_COUNT = 32;
extern uint16_t g_AbilityList[MAX_ABILITIES_COUNT];
extern uint8_t g_Ability[2];

extern bool g_DrawStatLockers;

extern uint32_t g_SelectedGameObjectHandle;

extern uint32_t g_ProcessStaticAnimationTimer;
extern uint32_t g_ProcessRemoveRangedTimer;
extern int g_MaxViewRange;
extern uint32_t g_GameFeaturesFlags;
inline bool Int32TryParse(const astr_t &str, int &result)
{
    char *end = nullptr;
    auto v = strtol(str.c_str(), &end, 10);
    if (!*end)
    {
        result = v;
        return true;
    }
    result = 0;
    return false;
    /*
    std::istringstream convert(str);
    try
    {
        convert >> result;
    }
    catch (int)
    {
        result = 0;
        return false;
    }
    if (!convert.eof())
    {
        result = 0;
        return false;
    }
    return true;
    */
}

inline bool IsBackground(int64_t flags)
{
    return (flags & 0x00000001) != 0;
}
inline bool IsWeapon(int64_t flags)
{
    return (flags & 0x00000002) != 0;
}
inline bool IsTransparent(int64_t flags)
{
    return (flags & 0x00000004) != 0;
}
inline bool IsTranslucent(int64_t flags)
{
    return (flags & 0x00000008) != 0;
}
inline bool IsWall(int64_t flags)
{
    return (flags & 0x00000010) != 0;
}
inline bool IsDamaging(int64_t flags)
{
    return (flags & 0x00000020) != 0;
}
inline bool IsImpassable(int64_t flags)
{
    return (flags & 0x00000040) != 0;
}
inline bool IsWet(int64_t flags)
{
    return (flags & 0x00000080) != 0;
}
inline bool IsUnknown(int64_t flags)
{
    return (flags & 0x00000100) != 0;
}
inline bool IsSurface(int64_t flags)
{
    return (flags & 0x00000200) != 0;
}
inline bool IsBridge(int64_t flags)
{
    return (flags & 0x00000400) != 0;
}
inline bool IsStackable(int64_t flags)
{
    return (flags & 0x00000800) != 0;
}
inline bool IsWindow(int64_t flags)
{
    return (flags & 0x00001000) != 0;
}
inline bool IsNoShoot(int64_t flags)
{
    return (flags & 0x00002000) != 0;
}
inline bool IsPrefixA(int64_t flags)
{
    return (flags & 0x00004000) != 0;
}
inline bool IsPrefixAn(int64_t flags)
{
    return (flags & 0x00008000) != 0;
}
inline bool IsPrefixThe(int64_t flags)
{
    return (flags & 0x0000C000) != 0;
}
inline bool IsInternal(int64_t flags)
{
    return (flags & 0x00010000) != 0;
}
inline bool IsFoliage(int64_t flags)
{
    return (flags & 0x00020000) != 0;
}
inline bool IsPartialHue(int64_t flags)
{
    return (flags & 0x00040000) != 0;
}
inline bool IsUseNewArt(int64_t flags)
{
    return (flags & 0x00080000) != 0;
}
inline bool IsMap(int64_t flags)
{
    return (flags & 0x00100000) != 0;
}
inline bool IsContainer(int64_t flags)
{
    return (flags & 0x00200000) != 0;
}
inline bool IsWearable(int64_t flags)
{
    return (flags & 0x00400000) != 0;
}
inline bool IsLightSource(int64_t flags)
{
    return (flags & 0x00800000) != 0;
}
inline bool IsAnimated(int64_t flags)
{
    return (flags & 0x01000000) != 0;
}
inline bool IsNoDiagonal(int64_t flags)
{
    return (flags & 0x02000000) != 0;
}
inline bool IsArtUsed(int64_t flags)
{
    return (flags & 0x04000000) != 0;
}
inline bool IsArmor(int64_t flags)
{
    return (flags & 0x08000000) != 0;
}
inline bool IsRoof(int64_t flags)
{
    return (flags & 0x10000000) != 0;
}
inline bool IsDoor(int64_t flags)
{
    return (flags & 0x20000000) != 0;
}
inline bool IsStairBack(int64_t flags)
{
    return (flags & 0x40000000) != 0;
}
inline bool IsStairRight(int64_t flags)
{
    return (flags & 0x80000000) != 0;
}

#if USE_PING
extern struct PING_INFO_DATA g_GameServerPingInfo;
extern uint32_t g_PingTimer;
#endif // USE_PING

extern uint32_t g_Ping;     // From packet 0x73
extern astr_t g_PingString; // Debug Info

#define IN_RANGE(name, id1, id2) ((name) >= (id1) && (name) <= (id2))
#define OUT_RANGE(name, id1, id2) ((name) < (id1) || (name) > (id2))

#define UO_USE_SHADER_FILES 0

#define UOFONT_SOLID 0x01
#define UOFONT_ITALIC 0x02
#define UOFONT_INDENTION 0x04    // Indent text (except 1 line)
#define UOFONT_BLACK_BORDER 0x08 // Black stroke
#define UOFONT_UNDERLINE 0x10    // Underlined
#define UOFONT_FIXED 0x20        // Fixed Length
#define UOFONT_CROPPED 0x40      // Fixed length with ellipsis
#define UOFONT_BQ 0x80           // Indent for the BQ tag

#define PLUGIN_FLAGS_PARSE_RECV 0x01
#define PLUGIN_FLAGS_PARSE_SEND 0x02
#define PLUGIN_FLAGS_RECV 0x04
#define PLUGIN_FLAGS_SEND 0x08
#define PLUGIN_FLAGS_EVENT_PROC 0x10
#define PLUGIN_FLAGS_CLIENT_ACCESS 0x20
#define PLUGIN_FLAGS_GAME_WORLD_DRAW 0x40
#define PLUGIN_FLAGS_SCENE_DRAW 0x80
#define PLUGIN_FLAGS_WORLD_MAP_DRAW 0x100

#define RANDOM_FIDGET_ANIMATION_DELAY (30000 + (rand() % 30000))
