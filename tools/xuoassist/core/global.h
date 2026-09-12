// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (global.h), Qt-diet per assistant-plan.md §4.2:
// QString -> std::string, QList/QVector -> std::vector, QTime/QElapsedTimer ->
// client g_Ticks (ms), QPoint -> local CPoint2Di. Assistant-owned globals only:
// the external-DLL host plumbing (g_DllPath/g_ClientPath/g_SaveAero/g_WineOS)
// is dropped; the host bridge function pointers stay and get wired to client
// internals by the packet-manager round.

#ifndef XUOASSIST_GLOBAL_H
#define XUOASSIST_GLOBAL_H

#include <stdint.h>
#include <assert.h>
#include <string>
#include <vector>

#include <common/utils.h> // countof
// Shared UO protocol types, owned by the client (same lineage as the Qt app's
// common/ headers): enums (CLIENT_VERSION, RACE_TYPE, OL_*, AT_*, VKI_*,
// PROMPT_TYPE, UPDATE_GAME_OBJECT_TYPE), FUNCDEF_* bridge typedefs,
// PACKET_PROC/UOI_SELECTED_TILE/WindowHandle.
#include <xuocore/enumlist.h>
#include <xuocore/commoninterfaces.h>
#include <xuocore/plugininterface.h>
#include "Globals.h" // client g_Ticks (ms) - supersedes the Qt app's QTime/g_Timer

namespace xuoassist {

using uchar = uint8_t;
using ushort = uint16_t;
using uint = uint32_t;

// Client tick source (plan §4.2: QTime/QElapsedTimer -> g_Ticks).
using ::g_Ticks;

using std::string;
using std::wstring;
using std::vector;

// Forward-ported from the Qt app's assist/assist.h with the assistant core
// round (Phase 1c); core/textconv.h previously carried the partial copy and
// now re-uses these.
enum FIND_TYPE_FLAGS
{
    FTF_NORMAL = 0x0000,
    FTF_NEAREST = 0x0001,
    FTF_FAST = 0x0002,
    FTF_MOBILE = 0x0004,
    FTF_ITEM = 0x0008,
    FTF_HUMAN = 0x0010,
    FTF_LIVE = 0x0020,
    FTF_DEAD = 0x0040,
    FTF_INJURED = 0x0080,
    FTF_NEXT = 0x0100,
    FTF_IGNORE_FRIENDS = 0x0200,
    FTF_IGNORE_ENEMIES = 0x0400
};

enum NOTORIETY_TYPE_FLAGS
{
    NTF_ALL = 0x00,
    NTF_INNOCENT = 0x01,
    NTF_FRIENDLY = 0x02,
    NTF_SOMEONE_GRAY = 0x04,
    NTF_CRIMINAL = 0x08,
    NTF_ENEMY = 0x10,
    NTF_MURDERER = 0x20,
    NTF_INVULNERABLE = 0x40
};

extern PACKET_PROC *g_SendClient;
extern PACKET_PROC *g_SendServer;
extern FUNCDEF_GET_STATIC_FLAGS *g_GetStaticFlags;
extern FUNCDEF_GET_VALUE_INT *g_GetInt;
extern FUNCDEF_SET_VALUE_INT *g_SetInt;
extern FUNCDEF_GET_VALUE_STRING *g_GetString;
extern FUNCDEF_SET_VALUE_STRING *g_SetString;
extern uint g_LastSendTime;
extern uint g_LastPacketTime;
extern uint g_TotalSendSize;
extern uint g_TotalRecvSize;
extern uchar g_LightLevel;
extern uchar g_PersonalLightLevel;

// Host bridge accessors; null-safe until the packet-manager round attaches the
// in-client implementations (the Qt app relied on the host DLL always being
// attached). Returning the default keeps pre-wiring behavior inert.
int GetInt(VALUE_KEY_INT key, int value = -1);
string GetString(VALUE_KEY_STRING key, const string &value = "");

enum XA_ENABLED_FEATURES
{
    XAEF_CORPSES_AUTOOPEN = 0x0000000000000001,
    XAEF_STEALTH_STEP_COUNTER = 0x0000000000000002,
    XAEF_SOUND_FILTER = 0x0000000000000004,
    XAEF_LIGHT_FILTER = 0x0000000000000008,
    XAEF_WEATHER_FILTER = 0x0000000000000010,
    XAEF_SEASON_FILTER = 0x0000000000000020,
    XAEF_NO_DEATH_SCREEN = 0x0000000000000040, // black screen ~5 sec
    XAEF_AUTO_OPEN_DOORS = 0x0000000000000080,
    XAEF_TRACKER = 0x0000000000000100,
    XAEF_LOOPED_MACRO = 0x0000000000000200,
    XAEF_EXTENDED_SCRIPTS = 0x0000000000000400,
    XAEF_FAST_ROTATION = 0x0000000000000800,
    XAEF_RECURSE_CONTAINERS_SEARCH = 0x0000000000001000,
    XAEF_AUTOSTART = 0x0000000000002000,
    XAEF_IGNORE_STAMINA_CHECK = 0x0000000000004000,
    XAEF_PARTY_AGENT = 0x0000000000008000,
    XAEF_SHOP_AGENT = 0x0000000000010000,
    XAEF_MINIMIZE_TO_TRAY = 0x0000000000020000,
    XAEF_SOUND_ECHO = 0x0000000000040000,
    XAEF_ANIMATION_ECHO = 0x0000000000080000,
    XAEF_EFFECT_ECHO = 0x0000000000100000,
    XAEF_MAP_ECHO = 0x0000000000200000,
    XAEF_SPEECH_FILTER = 0x0000000000400000,
    XAEF_TEXT_REPLACE = 0x0000000000800000,
    XAEF_RECONNECTOR = 0x0000000001000000,

    XAEF_ALL = 0xFFFFFFFFFFFFFFFF,

    XAEF_DEFAULT = XAEF_ALL
};

extern uint64_t g_EnabledFeature;

inline bool EnabledFeatureCorpsesAutoopen()
{
    return (g_EnabledFeature & XAEF_CORPSES_AUTOOPEN);
}
inline bool EnabledFeatureStealthStepCounter()
{
    return (g_EnabledFeature & XAEF_STEALTH_STEP_COUNTER);
}
inline bool EnabledFeatureSoundFilter()
{
    return (g_EnabledFeature & XAEF_SOUND_FILTER);
}
inline bool EnabledFeatureLightFilter()
{
    return (g_EnabledFeature & XAEF_LIGHT_FILTER);
}
inline bool EnabledFeatureWeatherFilter()
{
    return (g_EnabledFeature & XAEF_WEATHER_FILTER);
}
inline bool EnabledFeatureSeasonFilter()
{
    return (g_EnabledFeature & XAEF_SEASON_FILTER);
}
inline bool EnabledFeatureNoDeathScreen()
{
    return (g_EnabledFeature & XAEF_NO_DEATH_SCREEN);
}
inline bool EnabledFeatureAutoOpenDoors()
{
    return (g_EnabledFeature & XAEF_AUTO_OPEN_DOORS);
}
inline bool EnabledFeatureTracker()
{
    return (g_EnabledFeature & XAEF_TRACKER);
}
inline bool EnabledFeatureLoopedMacro()
{
    return (g_EnabledFeature & XAEF_LOOPED_MACRO);
}
inline bool EnabledFeatureExtendedScripts()
{
    return (g_EnabledFeature & XAEF_EXTENDED_SCRIPTS);
}
inline bool EnabledFeatureFastRotation()
{
    return (g_EnabledFeature & XAEF_FAST_ROTATION);
}
inline bool EnabledFeatureRecurseContainersSearch()
{
    return (g_EnabledFeature & XAEF_RECURSE_CONTAINERS_SEARCH);
}
inline bool EnabledFeatureAutostart()
{
    return (g_EnabledFeature & XAEF_AUTOSTART);
}
inline bool EnabledFeatureIgnoreStaminaCheck()
{
    return (g_EnabledFeature & XAEF_IGNORE_STAMINA_CHECK);
}
inline bool EnabledFeaturePartyAgent()
{
    return (g_EnabledFeature & XAEF_PARTY_AGENT);
}
inline bool EnabledFeatureShopAgent()
{
    return (g_EnabledFeature & XAEF_SHOP_AGENT);
}
inline bool EnabledFeatureMinimizeToTray()
{
    return (g_EnabledFeature & XAEF_MINIMIZE_TO_TRAY);
}
inline bool EnabledFeatureSoundEcho()
{
    return (g_EnabledFeature & XAEF_SOUND_ECHO);
}
inline bool EnabledFeatureAnimationEcho()
{
    return (g_EnabledFeature & XAEF_ANIMATION_ECHO);
}
inline bool EnabledFeatureEffectEcho()
{
    return (g_EnabledFeature & XAEF_EFFECT_ECHO);
}
inline bool EnabledFeatureMapEcho()
{
    return (g_EnabledFeature & XAEF_MAP_ECHO);
}
inline bool EnabledFeatureSpeechFilter()
{
    return (g_EnabledFeature & XAEF_SPEECH_FILTER);
}
inline bool EnabledFeatureTextReplace()
{
    return (g_EnabledFeature & XAEF_TEXT_REPLACE);
}
inline bool EnabledFeatureReconnector()
{
    return (g_EnabledFeature & XAEF_RECONNECTOR);
}

enum XA_ENABLED_COMMANDS : uint64_t
{
    XAEC_INFORMATION = 0x0000000000000001,
    XAEC_TEXT_WINDOW = 0x0000000000000002,
    XAEC_SEARCH_CONTAINER = 0x0000000000000004,
    XAEC_SEARCH_GROUND = 0x0000000000000008,
    XAEC_MOVING = 0x0000000000000010,
    XAEC_RESEND = 0x0000000000000020,
    XAEC_JOURNAL = 0x0000000000000040,
    XAEC_RENAME_MOUNTS = 0x0000000000000080,
    XAEC_SHOP_BUY = 0x0000000000000100,
    XAEC_SHOP_SELL = 0x0000000000000200,
    XAEC_HIDE = 0x0000000000000400,
    XAEC_MOVE_ITEMS = 0x0000000000000800,
    XAEC_MORPHING = 0x0000000000001000,
    XAEC_EMOTE_ACTIONS = 0x0000000000002000,
    XAEC_FIND_TYPE_OR_IGNORE = 0x0000000000004000,
    XAEC_FIND_ARRAYS = 0x0000000000008000,
    XAEC_OPTIONS_ACCESS = 0x0000000000010000,
    XAEC_SEARCH_CORPSE = 0x0000000000020000,
    XAEC_EXTENDED_MENU = 0x0000000000040000,
    XAEC_BUFFS = 0x0000000000080000,
    XAEC_SECURE_TRADING = 0x0000000000100000,
    XAEC_FRIEND_LIST = 0x0000000000200000,
    XAEC_ENEMY_LIST = 0x0000000000400000,
    XAEC_FONT_COLOR = 0x0000000000800000,
    XAEC_EXTENDED_TARGET_TILE = 0x0000000001000000,
    XAEC_LAUNCH = 0x0000000002000000,
    XAEC_REMOVED_PTR_POSITION = 0x0000000004000000,
    XAEC_EXTENDED_GUMP = 0x0000000008000000,
    XAEC_CHARACTER_PROPERTIES = 0x0000000010000000,
    XAEC_CHARACTER_PROFILE = 0x0000000020000000,
    XAEC_PROMPTS = 0x0000000040000000,
    XAEC_STATUSBARS = 0x0000000080000000,
    XAEC_LOG_OUT = 0x0000000100000000,
    XAEC_TIMERS = 0x0000000200000000,
    XAEC_CONTEXT_MENU = 0x0000000400000000,

    XAEC_ALL = 0xFFFFFFFFFFFFFFFF,

    XAEC_DEFAULT = XAEC_ALL
};

extern uint64_t g_EnabledCommands;

inline bool EnabledCommandInformation()
{
    return (g_EnabledCommands & XAEC_INFORMATION);
}
inline bool EnabledCommandTextWindow()
{
    return (g_EnabledCommands & XAEC_TEXT_WINDOW);
}
inline bool EnabledCommandSearchContainers()
{
    return (g_EnabledCommands & XAEC_SEARCH_CONTAINER);
}
inline bool EnabledCommandSearchGround()
{
    return (g_EnabledCommands & XAEC_SEARCH_GROUND);
}
inline bool EnabledCommandMoving()
{
    return (g_EnabledCommands & XAEC_MOVING);
}
inline bool EnabledCommandResend()
{
    return (g_EnabledCommands & XAEC_RESEND);
}
inline bool EnabledCommandJournal()
{
    return (g_EnabledCommands & XAEC_JOURNAL);
}
inline bool EnabledCommandRenameMounts()
{
    return (g_EnabledCommands & XAEC_RENAME_MOUNTS);
}
inline bool EnabledCommandShopBuy()
{
    return (g_EnabledCommands & XAEC_SHOP_BUY);
}
inline bool EnabledCommandShopSell()
{
    return (g_EnabledCommands & XAEC_SHOP_SELL);
}
inline bool EnabledCommandHide()
{
    return (g_EnabledCommands & XAEC_HIDE);
}
inline bool EnabledCommandMoveItems()
{
    return (g_EnabledCommands & XAEC_MOVE_ITEMS);
}
inline bool EnabledCommandMorphing()
{
    return (g_EnabledCommands & XAEC_MORPHING);
}
inline bool EnabledCommandEmoteActions()
{
    return (g_EnabledCommands & XAEC_EMOTE_ACTIONS);
}
inline bool EnabledCommandFindTypeOrIgnore()
{
    return (g_EnabledCommands & XAEC_FIND_TYPE_OR_IGNORE);
}
inline bool EnabledCommandFindArrays()
{
    return (g_EnabledCommands & XAEC_FIND_ARRAYS);
}
inline bool EnabledCommandOptionsAccess()
{
    return (g_EnabledCommands & XAEC_OPTIONS_ACCESS);
}
inline bool EnabledCommandSearchCorpse()
{
    return (g_EnabledCommands & XAEC_SEARCH_CORPSE);
}
inline bool EnabledCommandExtendedMenu()
{
    return (g_EnabledCommands & XAEC_EXTENDED_MENU);
}
inline bool EnabledCommandBuffs()
{
    return (g_EnabledCommands & XAEC_BUFFS);
}
inline bool EnabledCommandSecureTrading()
{
    return (g_EnabledCommands & XAEC_SECURE_TRADING);
}
inline bool EnabledCommandFriendList()
{
    return (g_EnabledCommands & XAEC_FRIEND_LIST);
}
inline bool EnabledCommandEnemyList()
{
    return (g_EnabledCommands & XAEC_ENEMY_LIST);
}
inline bool EnabledCommandFontColor()
{
    return (g_EnabledCommands & XAEC_FONT_COLOR);
}
inline bool EnabledCommandExtendedTargetTile()
{
    return (g_EnabledCommands & XAEC_EXTENDED_TARGET_TILE);
}
inline bool EnabledCommandLaunch()
{
    return (g_EnabledCommands & XAEC_LAUNCH);
}
inline bool EnabledCommandRemovedPtrPosition()
{
    return (g_EnabledCommands & XAEC_REMOVED_PTR_POSITION);
}
inline bool EnabledCommandExtendedGump()
{
    return (g_EnabledCommands & XAEC_EXTENDED_GUMP);
}
inline bool EnabledCommandCharacterProperties()
{
    return (g_EnabledCommands & XAEC_CHARACTER_PROPERTIES);
}
inline bool EnabledCommandCharacterProfile()
{
    return (g_EnabledCommands & XAEC_CHARACTER_PROFILE);
}
inline bool EnabledCommandPrompts()
{
    return (g_EnabledCommands & XAEC_PROMPTS);
}
inline bool EnabledCommandStatusbars()
{
    return (g_EnabledCommands & XAEC_STATUSBARS);
}
inline bool EnabledCommandLogOut()
{
    return (g_EnabledCommands & XAEC_LOG_OUT);
}
inline bool EnabledCommandTimers()
{
    return (g_EnabledCommands & XAEC_TIMERS);
}
inline bool EnabledCommandContextMenu()
{
    return (g_EnabledCommands & XAEC_CONTEXT_MENU);
}

enum XA_PACKET_MESSAGE_TYPE
{
    XAPMT_SET_FLAGS = 0x0001
};

enum TRADE_ACTIONS
{
    TA_OPEN = 0,
    TA_CLOSE,
    TA_CHECK
};

enum THIRD_PARTY_FEATURES
{
    TPF_FILTER_WEATHER = 1 << 0,
    TPF_FILTER_LIGHT = 1 << 1,

    TPF_SMART_TARGET = 1 << 2,
    TPF_RANGED_TARGET = 1 << 3,

    TPF_AUTOOPEN_DOORS = 1 << 4,

    TPF_UNEQUIP_ON_CAST = 1 << 5,
    TPF_AUTO_POTION_EQUIP = 1 << 6,

    TPF_PROTECT_HEALS = 1 << 7,

    TPF_LOOPED_MACROS = 1 << 8,

    TPF_USE_ONCE_AGENT = 1 << 9,
    TPF_RESTOCK_AGENT = 1 << 10,
    TPF_SELL_AGENT = 1 << 11,
    TPF_BUY_AGENT = 1 << 12,

    TPF_POTION_HOTKEYS = 1 << 13,

    TPF_RANDOM_TARGETS = 1 << 14,
    TPF_CLOSEST_TARGETS = 1 << 15, // All closest target hotkeys
    TPF_OVERHEAD_HEALTH = 1 << 16, // Health and Mana/Stam messages shown over player's heads

    TPF_AUTOLOOT_AGENT = 1 << 17,
    TPF_BONE_CUTTER_AGENT = 1 << 18,
    TPF_ADVANCED_MACROS = 1 << 19,
    TPF_AUTO_REMOUNT = 1 << 20,
    TPF_AUTO_BANDAGE = 1 << 21,
    TPF_ENEMY_TARGET_SHARE = 1 << 22,
    TPF_FILTER_SEASON = 1 << 23,
    TPF_SPELL_TARGET_SHARE = 1 << 24,

    TPF_ALL = 0xFFFFFFFF
};

struct SEASON_DATA
{
    uchar Index;
    uchar Music;
};

struct WEATHER_DATA
{
    uchar Type;
    uchar Count;
    uchar Temperature;
};

// Season/weather filter state written by the packet handlers (0x65/0xBC).
extern SEASON_DATA g_Season;
extern WEATHER_DATA g_Weather;

//! Replacement for the Qt app's QPoint globals (plan §4.2); x()/y() keep the
//! original call sites verbatim.
struct CPoint2Di
{
    int X{ 0 };
    int Y{ 0 };

    CPoint2Di() = default;
    CPoint2Di(int x, int y)
        : X(x)
        , Y(y)
    {
    }
    int x() const { return X; }
    int y() const { return Y; }
};

extern uint g_PlayerSerial;
extern uint g_Backpack;
extern uint g_FindItem;
extern uint g_CurrentMap;
extern string g_ServerName;
extern string g_CharacterName;
extern bool g_UpdateDisplayOnStep;
extern uint g_DressBag;
extern ushort g_MessagesColor;
extern uint g_LastTargetObject;
extern uint g_LastAttackObject;
extern ushort g_MorphGraphic;
extern ushort g_OriginalGraphic;
extern uint g_LastCorpseObject;
extern uint g_LastContainerObject;
//! Tick stamps (g_Ticks) replacing the Qt app's QTime objects.
extern uint g_LastUseObjectTimer;
extern uint g_StartTimer;
extern bool g_Connected;
extern uint g_LastFriendFound;
extern uint g_LastEnemyFound;
extern uchar g_Ability[2];
extern CPoint2Di g_LastTargetPosition;
extern CPoint2Di g_LastAttackPosition;
extern uint g_LastStatusRequest;
extern uint g_LastStatusObject;
extern uint g_LastUseObject;
extern UOI_SELECTED_TILE g_SelectedClientWorldObject;
extern bool g_ClientMacroPlayed;
extern CPoint2Di g_LastPlayerCorpsePosition;
extern uint g_LastPlayerDeadTimer;
extern int g_LastSkillIndex;
extern int g_LastSpellIndex;
extern bool g_TargetReceived;
extern bool g_MenuReceived;
extern bool g_GumpReceived;
extern bool g_PromptReceived;
extern bool g_ContextMenuReceived;
extern bool g_TradeReceived;
extern bool g_ShopReceived;
extern ushort g_SpellReagents[64][5];
extern CPoint2Di g_RemoveRangeXY;
extern uint g_LastTMapX;
extern uint g_LastTMapY;

static const bool g_LayerSafe[30] = { false, // 0
                                      true,  true, true,  true,  true,  true,  true,  true,
                                      true,  true, false, true,  true,  true,  false, false,
                                      true,  true, true,  true,
                                      false, // 0x15
                                      true,  true, true,  false, false, false, false, false };

const int MAX_ABILITIES_COUNT = 32;

//! Size of the static art.mul index table, >=CV_7090: count=0x10000, >=CV_7000:
//! count=0x8000, <CV_7000: count=0x4000
const int MAX_STATIC_DATA_INDEX_COUNT = 0x10000;

//! Size of the gump indexes table
const int MAX_GUMP_DATA_INDEX_COUNT = 0x10000;

const string g_AbilityName[MAX_ABILITIES_COUNT] = {
    "Armor Ignore",       "Bleed Attack",    "Concussion Blow",
    "Crushing Blow",      "Disarm",          "Dismount",
    "Double Strike",      "Infecting",       "Mortal Strike",
    "Moving Shot",        "Paralyzing Blow", "Shadow Strike",
    "Whirlwind Attack",
    "Riding Swipe", // CV_500a
    "Frenzied Whirlwind", "Block",           "Defense Mastery",
    "Nerve Strike",       "Talon Strike",    "Feint",
    "Dual Wield",         "Double Shot",     "Armor Pierce",
    "Bladeweave",         "Force Arrow",     "Lightning Arrow",
    "Psychic Attack",     "Serpent Arrow",   "Force of Nature",
    "Infused Throw", // CV_7000
    "Mystic Arc",         "Disrobe"
};

const int MAX_INVOKE_VIRTURE_COUNT = 3;

const string g_InvokeVirtureName[MAX_INVOKE_VIRTURE_COUNT] = { "Honor", "Sacrifice", "Valor" };

#ifndef IN_RANGE
#define IN_RANGE(name, id1, id2) ((name) >= (id1) && (name) <= (id2))
#endif
#ifndef OUT_RANGE
#define OUT_RANGE(name, id1, id2) ((name) < (id1) || (name) > (id2))
#endif

#ifndef RELEASE_POINTER
#define RELEASE_POINTER(ptr)                                                                       \
    if (ptr != nullptr)                                                                            \
    {                                                                                              \
        delete ptr;                                                                                \
        ptr = nullptr;                                                                             \
    }
#endif

//! Set/Get ordinary class property
#define SETGET(type, name, defaultValue)                                                           \
protected:                                                                                         \
    type m_##name{ defaultValue };                                                                 \
                                                                                                   \
public:                                                                                            \
    inline void Set##name(type val) { m_##name = val; }                                            \
    inline type Get##name() const { return m_##name; }

//! Set/Get ordinary class property
#define SETGETE(type, name, defaultValue, event)                                                   \
protected:                                                                                         \
    type m_##name{ defaultValue };                                                                 \
                                                                                                   \
public:                                                                                            \
    void event(const type &val);                                                                   \
    inline void Set##name(type val)                                                                \
    {                                                                                              \
        event(val);                                                                                \
        m_##name = val;                                                                            \
    }                                                                                              \
    inline type Get##name() const { return m_##name; }

//! Set/Get ordinary class property for pointers
#define SETGETP(type, name, defaultValue)                                                          \
protected:                                                                                         \
    type m_##name = defaultValue;                                                                  \
                                                                                                   \
public:                                                                                            \
    inline void Set##name(type val) { m_##name = val; }                                            \
    inline type Get##name() const { return m_##name; }

inline bool IsBackground(const uint64_t &flags)
{
    return (flags & 0x00000001);
}
inline bool IsWeapon(const uint64_t &flags)
{
    return (flags & 0x00000002);
}
inline bool IsTransparent(const uint64_t &flags)
{
    return (flags & 0x00000004);
}
inline bool IsTranslucent(const uint64_t &flags)
{
    return (flags & 0x00000008);
}
inline bool IsWall(const uint64_t &flags)
{
    return (flags & 0x00000010);
}
inline bool IsDamaging(const uint64_t &flags)
{
    return (flags & 0x00000020);
}
inline bool IsImpassable(const uint64_t &flags)
{
    return (flags & 0x00000040);
}
inline bool IsWet(const uint64_t &flags)
{
    return (flags & 0x00000080);
}
inline bool IsUnknown(const uint64_t &flags)
{
    return (flags & 0x00000100);
}
inline bool IsSurface(const uint64_t &flags)
{
    return (flags & 0x00000200);
}
inline bool IsBridge(const uint64_t &flags)
{
    return (flags & 0x00000400);
}
inline bool IsStackable(const uint64_t &flags)
{
    return (flags & 0x00000800);
}
inline bool IsWindow(const uint64_t &flags)
{
    return (flags & 0x00001000);
}
inline bool IsNoShoot(const uint64_t &flags)
{
    return (flags & 0x00002000);
}
inline bool IsPrefixA(const uint64_t &flags)
{
    return (flags & 0x00004000);
}
inline bool IsPrefixAn(const uint64_t &flags)
{
    return (flags & 0x00008000);
}
inline bool IsInternal(const uint64_t &flags)
{
    return (flags & 0x00010000);
}
inline bool IsFoliage(const uint64_t &flags)
{
    return (flags & 0x00020000);
}
inline bool IsPartialHue(const uint64_t &flags)
{
    return (flags & 0x00040000);
}
inline bool IsUnknown1(const uint64_t &flags)
{
    return (flags & 0x00080000);
}
inline bool IsMap(const uint64_t &flags)
{
    return (flags & 0x00100000);
}
inline bool IsContainer(const uint64_t &flags)
{
    return (flags & 0x00200000);
}
inline bool IsWearable(const uint64_t &flags)
{
    return (flags & 0x00400000);
}
inline bool IsLightSource(const uint64_t &flags)
{
    return (flags & 0x00800000);
}
inline bool IsAnimated(const uint64_t &flags)
{
    return (flags & 0x01000000);
}
inline bool IsNoDiagonal(const uint64_t &flags)
{
    return (flags & 0x02000000);
}
inline bool IsUnknown2(const uint64_t &flags)
{
    return (flags & 0x04000000);
}
inline bool IsArmor(const uint64_t &flags)
{
    return (flags & 0x08000000);
}
inline bool IsRoof(const uint64_t &flags)
{
    return (flags & 0x10000000);
}
inline bool IsDoor(const uint64_t &flags)
{
    return (flags & 0x20000000);
}
inline bool IsStairBack(const uint64_t &flags)
{
    return (flags & 0x40000000);
}
inline bool IsStairRight(const uint64_t &flags)
{
    return (flags & 0x80000000);
}

const string g_EquipScriptName = "equip_script";

const string g_LayerName[30] = {
    "None", // 0
    "RightHand", "LeftHand", "Shoes", "Pants", "Shirt",      "Helmet",   "Gloves", "Ring",
    "Talisman",  "Necklace", "Hair",  "Waist", "InnerTorso", "Bracelet", "Face",   "Beard",
    "MidTorso",  "Earrings", "Arms",  "Cloak",
    "Backpack", // 0x15
    "Robe",      "Eggs",     "Legs",  "Mount", "Buy",        "Resale",   "Sell",   "Bank"
};

inline uint unpack32(uchar *buf)
{
    return static_cast<uint>((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);
}

inline ushort unpack16(uchar *buf)
{
    return static_cast<ushort>((buf[0] << 8) | buf[1]);
}

inline void pack32(uchar *buf, uint x)
{
    buf[0] = uchar(x >> 24);
    buf[1] = uchar((x >> 16) & 0xff);
    buf[2] = uchar((x >> 8) & 0xff);
    buf[3] = uchar(x & 0xff);
}

inline void pack16(uchar *buf, ushort x)
{
    buf[0] = x >> 8;
    buf[1] = x & 0xff;
}

template <typename T, typename U> static inline T checked_cast(U value)
{
    auto result = static_cast<T>(value);
    assert(static_cast<U>(result) == value && "Type conversion loses information");
    return result;
}

template <typename T, typename U> static inline T checked_cast(U *value)
{
    auto result = checked_cast<T>((intptr_t)value);
    assert(static_cast<intptr_t>(result) == (intptr_t)value && "Type conversion loses information");
    return result;
}

} // namespace xuoassist

#endif // XUOASSIST_GLOBAL_H
