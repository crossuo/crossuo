// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (global.h), trimmed per assistant-plan.md §4.2:
// strings are astr_t, lists are std::vector, timers are
// client g_Ticks (ms), points are the client CPoint2Di. Assistant-owned globals only:
// the external-DLL host plumbing (g_DllPath/g_ClientPath/g_SaveAero/g_WineOS)
// is dropped; the host bridge function pointers stay and get wired to client
// internals by the packet-manager round.

#pragma once

#include <stdint.h>
#include <assert.h>
#include <string>
#include <vector>

#include <common/utils.h> // countof
#include <common/str.h> // astr_t / wstr_t (the client astr_t typedefs)
#include <xuocore/enumlist.h>
#include <xuocore/commoninterfaces.h>
#include "Globals.h" // client g_Ticks (ms) - supersedes the original app's tick timer

namespace xuoassist {

using uchar = uint8_t;
using ushort = uint16_t;
using uint = uint32_t;

// Client tick source (plan §4.2: the old timer objects -> g_Ticks).
using ::g_Ticks;

using std::vector;

// Forward-ported from the original app's assist/assist.h with the assistant core
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

extern FUNCDEF_GET_STATIC_FLAGS *g_GetStaticFlags;
extern FUNCDEF_GET_VALUE_INT *g_GetInt;
extern FUNCDEF_SET_VALUE_INT *g_SetInt;
extern uchar g_LightLevel;
extern uchar g_PersonalLightLevel;

// Host bridge accessors; null-safe until Bridge_Init (the first frame tick)
// attaches the in-client implementations. Returning the default keeps
// One-time wiring of the bridge pointers to the client implementations
// (g_Game accessors). Called from AssistantWindow_FrameTick before any
// script, hotkey or manager can consume them.
void Bridge_Init();

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

enum TRADE_ACTIONS
{
    TA_OPEN = 0,
    TA_CLOSE,
    TA_CHECK
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

// Points are the client type (Point.h); the assistant only reads/writes X/Y.
using ::CPoint2Di;

struct SelectedTile
{
    unsigned int Serial;
    unsigned short Graphic;
    unsigned short Color;
    int X;
    int Y;
    int Z;
    unsigned short LandGraphic;
    int LandX;
    int LandY;
    int LandZ;
};

// The client-owned player serial; the client PacketManager keeps it current
// post-login.
using ::g_PlayerSerial;
extern uint g_Backpack;
// The client-owned current facet; ChangeMap updates it.
using ::g_CurrentMap;
extern bool g_UpdateDisplayOnStep;
extern uint g_DressBag;
extern ushort g_MessagesColor;
extern ushort g_MorphGraphic;
extern ushort g_OriginalGraphic;
extern uint g_LastCorpseObject;
extern uint g_LastContainerObject;
//! Tick stamps (g_Ticks) replacing the original app's timer objects.
extern uint g_LastUseObjectTimer;
extern bool g_Connected;
extern uint g_LastFriendFound;
extern uint g_LastEnemyFound;
// The client-owned combat ability table (GamePlayer/CrossUO keep it current).
using ::g_Ability;
extern CPoint2Di g_LastTargetPosition;
extern CPoint2Di g_LastAttackPosition;
extern uint g_LastStatusRequest;
extern uint g_LastStatusObject;
extern uint g_LastUseObject;
extern SelectedTile g_SelectedClientWorldObject;
extern CPoint2Di g_LastPlayerCorpsePosition;
extern uint g_LastPlayerDeadTimer;
// The client-owned last-cast/last-used indexes (client send sites update
// them before the macro Record taps run).
using ::g_LastSkillIndex;
using ::g_LastSpellIndex;
extern bool g_TargetReceived;
extern bool g_MenuReceived;
extern bool g_GumpReceived;
extern bool g_PromptReceived;
extern bool g_ContextMenuReceived;
extern bool g_TradeReceived;
extern bool g_ShopReceived;
extern uint g_LastTMapX;
extern uint g_LastTMapY;

static const bool g_LayerSafe[30] = { false, // 0
                                      true,  true, true,  true,  true,  true,  true,  true,
                                      true,  true, false, true,  true,  true,  false, false,
                                      true,  true, true,  true,
                                      false, // 0x15
                                      true,  true, true,  false, false, false, false, false };

// MAX_ABILITIES_COUNT, MAX_STATIC_DATA_INDEX_COUNT and MAX_GUMP_DATA_INDEX_COUNT
// come from Globals.h / xuocore/enumlist.h.

const int MAX_INVOKE_VIRTURE_COUNT = 3;

const astr_t g_InvokeVirtureName[MAX_INVOKE_VIRTURE_COUNT] = { "Honor", "Sacrifice", "Valor" };

const astr_t g_LayerName[30] = {
    "None", // 0
    "RightHand", "LeftHand", "Shoes", "Pants", "Shirt",      "Helmet",   "Gloves", "Ring",
    "Talisman",  "Necklace", "Hair",  "Waist", "InnerTorso", "Bracelet", "Face",   "Beard",
    "MidTorso",  "Earrings", "Arms",  "Cloak",
    "Backpack", // 0x15
    "Robe",      "Eggs",     "Legs",  "Mount", "Buy",        "Resale",   "Sell",   "Bank"
};

} // namespace xuoassist
