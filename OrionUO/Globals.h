// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Point.h"
#include "Constants.h"
#include "plugin/enumlist.h"
#include "Backend.h" // CGLTexture, GLuint

class CGump;

#define countof(xarray) (sizeof(xarray) / sizeof(xarray[0]))

enum
{
    WINDOW_INACTIVE = 0,
    WINDOW_ACTIVE = 1,
};

extern bool g_AltPressed;
extern bool g_CtrlPressed;
extern bool g_ShiftPressed;
extern bool g_MovingFromMouse;
extern bool g_AutoMoving;
extern bool g_AbyssPacket03First;

bool CanBeDraggedByOffset(const CPoint2Di &point);
void TileOffsetOnMonitorToXY(int &ofsX, int &ofsY, int &x, int &y);
string ToCamelCase(string text);

class CGameObject;
int GetDistance(CGameObject *current, CGameObject *target);
int GetDistance(CGameObject *current, const CPoint2Di &target);
bool CheckMultiDistance(const CPoint2Di &current, CGameObject *target, int maxDistance);
int GetDistance(const CPoint2Di &current, CGameObject *target);
int GetDistance(const CPoint2Di &current, const CPoint2Di &target);
int GetRemoveDistance(const CPoint2Di &current, CGameObject *target);
int GetTopObjDistance(CGameObject *current, CGameObject *target);

const char *GetReagentName(uint16_t id);

extern int g_LandObjectsCount;
extern int g_StaticsObjectsCount;
extern int g_GameObjectsCount;
extern int g_MultiObjectsCount;
extern int g_RenderedObjectsCountInGameWindow;

extern GLdouble g_GlobalScale;

extern CGLTexture g_MapTexture[MAX_MAPS_COUNT];

extern CGLTexture g_AuraTexture;

extern bool g_LogoutAfterClick;

extern int g_FrameDelay[2];

extern uint32_t g_LastSendTime;

extern uint32_t g_LastPacketTime;

extern uint32_t g_TotalSendSize;

extern uint32_t g_TotalRecvSize;

extern uint32_t g_Ticks;

extern GLuint ShaderColorTable;
extern GLuint g_ShaderDrawMode;

extern string g_Language;

extern GAME_STATE g_GameState;

extern CGLTexture g_TextureGumpState[2];

extern CSize g_MapSize[MAX_MAPS_COUNT];
extern CSize g_MapBlockSize[MAX_MAPS_COUNT];

extern int g_MultiIndexCount;

extern class CGLFrameBuffer g_LightBuffer;

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

typedef vector<pair<uint32_t, uint32_t>> UINTS_PAIR_LIST;

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

extern uint32_t g_Ping;

extern bool g_DrawAura;

static const int MAX_ABILITIES_COUNT = 32;
extern uint16_t g_AbilityList[MAX_ABILITIES_COUNT];
extern uint8_t g_Ability[2];

extern bool g_DrawStatLockers;

extern uint32_t g_SelectedGameObjectHandle;

extern bool g_ShowWarnings;

extern uint32_t g_ProcessStaticAnimationTimer;
extern uint32_t g_ProcessRemoveRangedTimer;
extern int g_MaxViewRange;
extern uint32_t g_OrionFeaturesFlags;
extern struct PING_INFO_DATA g_GameServerPingInfo;
extern string g_PingString;
extern uint32_t g_PingTimer;
inline bool Int32TryParse(const string &str, int &result)
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
inline bool IsUnknown1(int64_t flags)
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
inline bool IsUnknown2(int64_t flags)
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

template <typename T, typename U>
static inline T checked_cast(U value)
{
    auto result = static_cast<T>(value);
    assert(static_cast<U>(result) == value && "Type conversion loses information");
    return result;
}

template <typename T, typename U>
static inline T checked_cast(U *value)
{
    auto result = checked_cast<T>((intptr_t)value);
    assert(static_cast<intptr_t>(result) == (intptr_t)value && "Type conversion loses information");
    return result;
}

template <typename T>
static inline std::unique_ptr<T> unique_cast(void *value)
{
    return std::unique_ptr<T>((T *)value);
}

struct AutoFree
{
    AutoFree(void *p)
        : _p(p)
    {
    }
    ~AutoFree()
    {
        if (_p)
        {
            free(_p);
        }
    }

private:
    void *_p = nullptr;
};
