// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The assistant JS API (Phase 4b, batches 1+2). Natives run on the SCRIPT
// thread and marshal through ScriptQueue_Exec; the drain-side bodies of every
// tag live in the ScriptCommand_Execute switch at the bottom and run on the
// MAIN thread (they port the original OnCommand* handler bodies of
// managers/commandmanager.cpp and the direct reads of scriptengine.cpp + the
// engine classes gumpengine/textwindowengine/menuengine/fileengine/
// clientmacroengine/clientselectedtile.cpp).
//
// API classification (plan rule: everything queued unless obviously safe):
//   [Q] mutating calls   -> queued; main thread executes the ported handler
//   [G] state getters    -> queued; main thread reads and fills the result
//   [D] direct reads     -> only Now() (g_Ticks), Time/Date/Random/
//                           AssistVersion (pure libc), engine pause/terminate
//                           flags - and the engine-class objects' OWN copies
//                           (GetGump/GetMenu deep-copy the manager records
//                           into the command result; the JS wrapper owns the
//                           copy, like the original CScriptGumpObject ctor copy).
// The journal mirror (journal.cpp) is main-thread fed/read; script threads
// receive copies made inside the drain.
//
// Batch-2 surface (this file): gump hook/count/get/select, menu wait/count/
// get/select/close, prompt wait/state/send, trade, shop (Buy/Sell), the
// WaitFor* flag family, timers, context menu, client macros, the text window
// (scriptgump.cpp), the sandboxed file engine (scriptfile.cpp), buffs
// (scriptstate.cpp) and the globals store. Player. covers the CPlayerObject
// list.

#include "apiscript.h"
#include "commandqueue.h"
#include "engine.h"
#include "journal.h"
#include "scriptfile.h"
#include "scriptgump.h"
#include "scriptstate.h"
#include "scripthandle.h"

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

extern "C" {
#include "quickjs/quickjs.h"
}

#include "../config/assistantconfig.h"
#include "../core/actions.h"
#include "../core/assist.h"
#include "../core/dressagent.h"
#include "../core/target.h"
#include "../core/textcommandmanager.h"
#include "../managers/skillmanager.h"
#include "../managers/objectpropertiesmanager.h"
#include "../managers/spellmanager.h"
#include "../managers/gumpmanager.h"
#include "../managers/menumanager.h"
#include "../managers/promptmanager.h"
#include "../managers/trademanager.h"
#include "../managers/contextmenumanager.h"
#include "../core/shopagent.h"
#include "../model/commonitems/skill.h"
#include "../model/commonitems/gump.h"
#include "../model/commonitems/gumphook.h"
#include "../model/commonitems/menu.h"
#include "../model/gameobjects/gamecharacter.h"
#include "../model/gameobjects/gameitem.h"
#include "../model/gameobjects/gameplayer.h"
#include "../model/gameobjects/gameworld.h"

#include "../core/packets.h" // the assistant packet wrappers
#include "Network/Packets.h" // the client packets the wrappers build on
#include "ScreenshotBuilder.h" // g_ScreenshotBuilder (Client.Screenshot)
#include "Walker/PathFinder.h" // g_PathFinder (client walker)

#include <chrono>
#include <random>
#include <cstring>
#include <ctime>
#include <thread>

#include <revision.h> // PRODUCT_VERSION_STR

namespace xuoassist {

// ---------------------------------------------------------------------------
// script-thread helpers
// ---------------------------------------------------------------------------

namespace {

enum XA_SAY_KIND
{
    SAY_NORMAL = 0,
    SAY_PRINT,
    SAY_CHARPRINT,
    SAY_YELL,
    SAY_WHISPER,
    SAY_EMOTE,
    SAY_BROADCAST,
    SAY_GUILD,
    SAY_ALLIANCE,
    SAY_PARTY
};

enum XA_DRESS_KIND
{
    DK_SET_DRESS_BAG = 0,
    DK_UNSET_DRESS_BAG,
    DK_SET_ARM,
    DK_UNSET_ARM,
    DK_SET_DRESS,
    DK_UNSET_DRESS,
    DK_ARM,
    DK_DISARM,
    DK_DRESS,
    DK_UNDRESS,
    DK_UNEQUIP,
    DK_EQUIP,
    DK_EQUIP_T
};

enum XA_MOVE_KIND
{
    MK_MOVE_ITEM = 0,
    MK_DROP,
    MK_DROP_HERE
};

enum XA_USE_KIND
{
    UK_USE_TYPE = 0,
    UK_USE_FROM_GROUND,
    UK_USE_TYPE_LIST,
    UK_USE_FROM_GROUND_LIST
};

enum XA_HOOK_KIND
{
    HK_WAIT_OBJECT = 0,
    HK_TARGET_OBJECT,
    HK_WAIT_TYPE,
    HK_TARGET_TYPE,
    HK_WAIT_GROUND,
    HK_TARGET_GROUND,
    HK_WAIT_TYPE_LIST,
    HK_TARGET_TYPE_LIST,
    HK_WAIT_GROUND_LIST,
    HK_TARGET_GROUND_LIST,
    HK_WAIT_TILE,
    HK_TARGET_TILE,
    HK_WAIT_TILE_REL,
    HK_TARGET_TILE_REL
};
XA_SCRIPT_COMMAND *sc_make(XA_SCRIPT_COMMAND_TAG tag)
{
    auto *command = new XA_SCRIPT_COMMAND();
    command->Tag = tag;
    return command;
}

// Allocates, queues and waits; the caller reads Result* and deletes.
XA_SCRIPT_COMMAND *sc_exec(XA_SCRIPT_COMMAND_TAG tag)
{
    auto *command = sc_make(tag);
    ScriptQueue_Exec(command);
    return command;
}

int result_int(XA_SCRIPT_COMMAND *command, int fallback = 0)
{
    const int value = command->ResultDropped ? fallback : command->ResultInt;
    delete command;
    return value;
}

bool result_bool(XA_SCRIPT_COMMAND *command, bool fallback = false)
{
    const bool value = command->ResultDropped ? fallback : command->ResultBool;
    delete command;
    return value;
}

uint result_serial(XA_SCRIPT_COMMAND *command, uint fallback = 0)
{
    const uint value = command->ResultDropped ? fallback : command->ResultSerial;
    delete command;
    return value;
}

vector<string> result_list(XA_SCRIPT_COMMAND *command)
{
    vector<string> value;
    if (!command->ResultDropped)
    {
        value = command->ResultList;
    }
    delete command;
    return value;
}

// JS argument helpers -------------------------------------------------------

string arg_text(JSContext *ctx, JSValueConst *argv, int argc, int index, const string &def)
{
    if (index >= argc)
    {
        return def;
    }
    size_t size = 0;
    const char *text = JS_ToCStringLen(ctx, &size, argv[index]);
    if (text == nullptr)
    {
        JS_FreeValue(ctx, JS_GetException(ctx)); // swallow, keep the default
        return def;
    }
    string result(text, size);
    JS_FreeCString(ctx, text);
    return result;
}

int arg_int(JSContext *ctx, JSValueConst *argv, int argc, int index, int def)
{
    if (index >= argc)
    {
        return def;
    }
    int32_t value = def;
    if (JS_ToInt32(ctx, &value, argv[index]) != 0)
    {
        JS_FreeValue(ctx, JS_GetException(ctx)); // bad arg type: keep the default
        def = value = def;
    }
    return value;
}

bool arg_bool(JSContext *ctx, JSValueConst *argv, int argc, int index, bool def)
{
    if (index >= argc)
    {
        return def;
    }
    const int value = JS_ToBool(ctx, argv[index]);
    return value < 0 ? def : value != 0;
}

vector<string> arg_list(JSContext *ctx, JSValueConst *argv, int argc, int index)
{
    vector<string> list;
    if (index >= argc)
    {
        return list;
    }
    JSValueConst value = argv[index];
    if (JS_IsArray(value))
    {
        JSValue lengthValue = JS_GetPropertyStr(ctx, value, "length");
        int32_t length = 0;
        JS_ToInt32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);
        for (int32_t i = 0; i < length; i++)
        {
            JSValue item = JS_GetPropertyUint32(ctx, value, (uint32_t)i);
            size_t size = 0;
            const char *text = JS_ToCStringLen(ctx, &size, item);
            if (text != nullptr)
            {
                list.push_back(string(text, size));
                JS_FreeCString(ctx, text);
            }
            else
            {
                JS_FreeValue(ctx, JS_GetException(ctx));
            }
            JS_FreeValue(ctx, item);
        }
    }
    else
    {
        list.push_back(arg_text(ctx, argv, argc, index, ""));
    }
    return list;
}

// The pause gate: the engine calls PauseWaiting() after nearly every
// marshaled call.
// QuickJS' interrupt handler covers pauses INSIDE js loops; this covers the
// between-calls ones.
void pause_gate(JSContext *ctx)
{
    CScriptEngine *engine = (CScriptEngine *)JS_GetContextOpaque(ctx);
    if (engine == nullptr)
    {
        return;
    }
    while (engine->GetPaused() && !engine->GetTerminated())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// [Q] the generic marshaled mutation: command name + pre-split args executed
// by CTextCommandManager::RunCommand on the main thread (the same handler
// body the chat command runs, incl. config gating + sentinel resolution).
void run_command(JSContext *ctx, const char *name, const vector<string> &args = {})
{
    auto *command = sc_make(SC_TEXT_CMD);
    command->TextA = name;
    command->ListA = args;
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
}

string int_to_text(int value)
{
    char buf[32] = { 0 };
    snprintf(buf, sizeof(buf), "%i", value);
    return buf;
}

// ---------------------------------------------------------------------------
// JS classes (GameObject = CScriptGameObject, JournalEntry =
// CScriptJournalObject - script/gameobjectengine.h + journalengine.h)
// ---------------------------------------------------------------------------

JSClassID s_GameObjectClass = 0;
JSClassID s_JournalClass = 0;

typedef struct
{
    uint Serial;
} XA_JS_GAMEOBJECT;

typedef struct
{
    CJournalMessage *Message;
} XA_JS_JOURNALENTRY;

void gameobject_finalizer(JSRuntime *rt, JSValue val)
{
    auto *data = (XA_JS_GAMEOBJECT *)JS_GetOpaque(val, s_GameObjectClass);
    delete data;
    (void)rt;
}

void journal_finalizer(JSRuntime *rt, JSValue val)
{
    auto *data = (XA_JS_JOURNALENTRY *)JS_GetOpaque(val, s_JournalClass);
    delete data->Message;
    delete data;
    (void)rt;
}

// --- the 4b-2 engine classes --------------------------------------------------
// Gump/Menu hold DEEP COPIES made inside the drain (the original CScriptGumpObject /
// CScriptMenuObject ctors copied the manager records the same way); GumpHook /
// ClientMacro / File are pure script-side records. All script-thread owned.

JSClassID s_GumpClass = 0;
JSClassID s_MenuClass = 0;
JSClassID s_GumpHookClass = 0;
JSClassID s_ClientMacroClass = 0;
JSClassID s_FileClass = 0;

// opaque = CGump* (deep copy; owned)
void gump_finalizer(JSRuntime *rt, JSValue val)
{
    auto *gump = (CGump *)JS_GetOpaque(val, s_GumpClass);
    delete gump;
    (void)rt;
}

// opaque = CMenu* (deep copy; owned)
void menu_finalizer(JSRuntime *rt, JSValue val)
{
    auto *menu = (CMenu *)JS_GetOpaque(val, s_MenuClass);
    delete menu;
    (void)rt;
}

typedef std::pair<int, string> XA_GUMPHOOK_ENTRY;
typedef std::pair<int, bool> XA_GUMPHOOK_CHECK;

// named struct (not an anonymous typedef): the type crosses a linkage
// boundary through the registration table's finalizer pointer
struct XA_JS_GUMPHOOK
{
    int Index{ 0 };
    vector<XA_GUMPHOOK_ENTRY> Entries;
    vector<XA_GUMPHOOK_CHECK> Checks;
};

struct XA_JS_CLIENTMACRO
{
    vector<string> Actions;
    vector<string> SubActions;
};

void gumphook_finalizer(JSRuntime *rt, JSValue val)
{
    auto *hook = (XA_JS_GUMPHOOK *)JS_GetOpaque(val, s_GumpHookClass);
    delete hook;
    (void)rt;
}

void clientmacro_finalizer(JSRuntime *rt, JSValue val)
{
    auto *macro = (XA_JS_CLIENTMACRO *)JS_GetOpaque(val, s_ClientMacroClass);
    delete macro;
    (void)rt;
}

void file_finalizer(JSRuntime *rt, JSValue val)
{
    auto *file = (XA_SCRIPT_FILE *)JS_GetOpaque(val, s_FileClass);
    if (file != nullptr)
    {
        ScriptFile_Close(file);
    }
    delete file;
    (void)rt;
}

JSValue new_gump_object(JSContext *ctx, CGump *copy)
{
    JSValue obj = JS_NewObjectClass(ctx, s_GumpClass);
    if (JS_IsException(obj))
    {
        delete copy;
        return obj;
    }
    JS_SetOpaque(obj, copy);
    return obj;
}

JSValue new_menu_object(JSContext *ctx, CMenu *copy)
{
    JSValue obj = JS_NewObjectClass(ctx, s_MenuClass);
    if (JS_IsException(obj))
    {
        delete copy;
        return obj;
    }
    JS_SetOpaque(obj, copy);
    return obj;
}

// Object-getter selector (SubTag of SC_OBJECT_GET / SC_PLAYER_GET; the drain
// switch reads the same logical field of the world object - one ported body
// per CScriptGameObject/CPlayerObject getter).
enum XA_OBJ_FIELD
{
    OF_TEXT_SERIAL = 1,
    OF_TEXT_GRAPHIC,
    OF_TEXT_COLOR,
    OF_TEXT_CONTAINER,
    OF_TEXT_NAME,
    OF_INT_X,
    OF_INT_Y,
    OF_INT_Z,
    OF_INT_MAP,
    OF_INT_COUNT,
    OF_INT_FLAGS,
    OF_BOOL_MOBILE,
    OF_BOOL_IGNORED,
    OF_BOOL_FROZEN,
    OF_BOOL_POISONED,
    OF_BOOL_FLYING,
    OF_BOOL_YELLOWHITS,
    OF_BOOL_IGNORECHARS,
    OF_BOOL_LOCKED,
    OF_BOOL_WARMODE,
    OF_BOOL_HIDDEN,
    OF_BOOL_ISHUMAN,
    OF_BOOL_ISPLAYER,
    OF_BOOL_ISCORPSE,
    OF_INT_LAYER,
    OF_BOOL_ISMULTI,
    OF_INT_EQUIPLAYER,
    OF_INT_HITS,
    OF_INT_MAXHITS,
    OF_INT_MANA,
    OF_INT_MAXMANA,
    OF_INT_STAM,
    OF_INT_MAXSTAM,
    OF_BOOL_FEMALE,
    OF_INT_RACE,
    OF_INT_DIRECTION,
    OF_INT_NOTORIETY,
    OF_BOOL_CANCHANGENAME,
    OF_BOOL_DEAD,
    OF_BOOL_EXISTS,
    OF_TEXT_PROPERTIES,
    OF_BOOL_PROFILERECV,
    OF_TEXT_PROFILE,
    OF_TEXT_TITLE,
    // player-only (CGamePlayer fields)
    OF_INT_STR,
    OF_INT_INT,
    OF_INT_DEX,
    OF_INT_LOCKSTR,
    OF_INT_LOCKINT,
    OF_INT_LOCKDEX,
    OF_INT_WEIGHT,
    OF_INT_MAXWEIGHT,
    OF_INT_ARMOR,
    OF_INT_GOLD,
    OF_INT_STATSCAP,
    OF_INT_FOLLOWERS,
    OF_INT_MAXFOLLOWERS,
    OF_INT_FIRERES,
    OF_INT_COLDRES,
    OF_INT_POISONRES,
    OF_INT_ENERGYRES,
    OF_INT_LUCK,
    OF_INT_MINDMG,
    OF_INT_MAXDMG,
    OF_INT_TITHING,
    OF_INT_STEALTH
};

JSValue new_game_object(JSContext *ctx, uint serial)
{
    auto *data = new XA_JS_GAMEOBJECT();
    data->Serial = serial;
    JSValue obj = JS_NewObjectClass(ctx, s_GameObjectClass);
    if (JS_IsException(obj))
    {
        delete data;
        return obj;
    }
    JS_SetOpaque(obj, data);
    return obj;
}

JSValue new_journal_entry(JSContext *ctx, const CJournalMessage &message)
{
    auto *data = new XA_JS_JOURNALENTRY();
    data->Message = new CJournalMessage(message);
    JSValue obj = JS_NewObjectClass(ctx, s_JournalClass);
    if (JS_IsException(obj))
    {
        delete data->Message;
        delete data;
        return obj;
    }
    JS_SetOpaque(obj, data);
    return obj;
}

template <typename T, size_t N>
constexpr size_t count_of(const T (&)[N])
{
    return N;
}

XA_JS_GAMEOBJECT *gameobject_of(JSContext *ctx, JSValueConst obj)
{
    return (XA_JS_GAMEOBJECT *)JS_GetOpaque2(ctx, obj, s_GameObjectClass);
}

JSValue field_to_js(JSContext *ctx, XA_SCRIPT_COMMAND *command, int magic)
{
    switch (magic)
    {
        case OF_TEXT_SERIAL:
        case OF_TEXT_GRAPHIC:
        case OF_TEXT_COLOR:
        case OF_TEXT_CONTAINER:
        case OF_TEXT_NAME:
        case OF_TEXT_PROPERTIES:
        case OF_TEXT_PROFILE:
        case OF_TEXT_TITLE:
            return JS_NewStringLen(ctx, command->ResultText.c_str(), command->ResultText.size());
        case OF_BOOL_MOBILE:
        case OF_BOOL_IGNORED:
        case OF_BOOL_FROZEN:
        case OF_BOOL_POISONED:
        case OF_BOOL_FLYING:
        case OF_BOOL_YELLOWHITS:
        case OF_BOOL_IGNORECHARS:
        case OF_BOOL_LOCKED:
        case OF_BOOL_WARMODE:
        case OF_BOOL_HIDDEN:
        case OF_BOOL_ISHUMAN:
        case OF_BOOL_ISPLAYER:
        case OF_BOOL_ISCORPSE:
        case OF_BOOL_ISMULTI:
        case OF_BOOL_FEMALE:
        case OF_BOOL_CANCHANGENAME:
        case OF_BOOL_DEAD:
        case OF_BOOL_EXISTS:
        case OF_BOOL_PROFILERECV:
            return JS_NewBool(ctx, !command->ResultDropped && command->ResultBool);
        default:
            return JS_NewInt32(ctx, command->ResultDropped ? 0 : command->ResultInt);
    }
}

// [G] the generic getter: one queued SC_OBJECT_GET per call, selector in
// magic, target serial from `this`.
JSValue obj_get(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic)
{
    (void)argc;
    (void)argv;
    auto *data = gameobject_of(ctx, thisVal);
    if (data == nullptr)
    {
        return JS_EXCEPTION;
    }
    auto *command = sc_make(SC_OBJECT_GET);
    command->SubTag = magic;
    command->SerialA = data->Serial;
    ScriptQueue_Exec(command);
    JSValue result = field_to_js(ctx, command, magic);
    delete command;
    return result;
}

// [G] Hits("%")/Mana("%")/Stam("%")/Weight("%"): two queued reads + the same
// (value * 100) / max formula as CPlayerObject::Hits.
JSValue obj_get_percent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic)
{
    (void)argc;
    (void)argv;
    auto *data = gameobject_of(ctx, thisVal);
    if (data == nullptr)
    {
        return JS_EXCEPTION;
    }
    auto *valueCommand = sc_make(SC_OBJECT_GET);
    valueCommand->SubTag = magic;
    valueCommand->SerialA = data->Serial;
    ScriptQueue_Exec(valueCommand);
    const int value = valueCommand->ResultDropped ? 0 : valueCommand->ResultInt;
    delete valueCommand;

    auto *maxCommand = sc_make(SC_OBJECT_GET);
    maxCommand->SubTag = magic + 1; // HITS->MAXHITS etc. are adjacent
    maxCommand->SerialA = data->Serial;
    ScriptQueue_Exec(maxCommand);
    const int maxValue = maxCommand->ResultDropped ? 0 : maxCommand->ResultInt;
    delete maxCommand;

    if (maxValue == 0)
    {
        return JS_NewInt32(ctx, 0);
    }
    return JS_NewInt32(ctx, (value * 100) / maxValue);
}

// [Q] object actions on a found instance (Click/UseObject/RequestName): the
// same marshaled call with the object's serial as the argument.
JSValue obj_action(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic)
{
    (void)argc;
    (void)argv;
    auto *data = gameobject_of(ctx, thisVal);
    if (data == nullptr)
    {
        return JS_EXCEPTION;
    }
    const char *name = magic == 0 ? "click" : magic == 1 ? "useobject" : "requestname";
    run_command(ctx, name, { CAssistant::SerialToText(data->Serial) });
    return JS_UNDEFINED;
}

// ---------------------------------------------------------------------------
// [Q] natives - each ports the same-named CScriptEngine method
// (script/scriptengine.cpp): `emit g_CommandManager.CommandX(...);
// PauseWaiting();` becomes fill-command / queue+wait / pause_gate().
// ---------------------------------------------------------------------------

JSValue jsn_wait(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    // [Q] CScriptEngine::Wait: the delay comes from
    // the config text table (DelayFromText) + raw ms; the sleep loop checks
    // pause/terminate every 10 ms (the old timer granularity).
    const string text = arg_text(ctx, argv, argc, 0, "");
    auto *parse = sc_make(SC_WAIT_PARSE);
    parse->TextA = text;
    ScriptQueue_Exec(parse);
    int delay = parse->ResultDropped ? 0 : parse->ResultInt;
    delete parse;

    CScriptEngine *engine = (CScriptEngine *)JS_GetContextOpaque(ctx);
    const int step = 10;
    while (delay > 0 && engine != nullptr && !engine->GetTerminated())
    {
        if (engine->GetPaused())
        {
            pause_gate(ctx);
            continue;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(step));
        delay -= step;
    }
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] a whole family of single-purpose commands share this native; magic
// indexes the text-command table (RunCommand executes the exact ported body).
enum XA_SIMPLE_CMD
{
    XC_INFO = 0,
    XC_INFOTILE,
    XC_INFOMENU,
    XC_SAVECONFIG,
    XC_CLICK,
    XC_USEOBJECT,
    XC_GETSTATUS,
    XC_ATTACK,
    XC_HELPGUMP,
    XC_CLOSEUO,
    XC_WARMODE,
    XC_RESEND,
    XC_EMOTEACTION,
    XC_HIDE,
    XC_IGNORE,
    XC_IGNORERESET,
    XC_SETDRESSBAG,
    XC_UNSETDRESSBAG,
    XC_SETARM,
    XC_UNSETARM,
    XC_SETDRESS,
    XC_UNSETDRESS,
    XC_ADDTYPE,
    XC_REMOVETYPE,
    XC_ADDOBJECT,
    XC_REMOVEOBJECT,
    XC_SHOWJOURNAL,
    XC_CLEARJOURNAL,
    XC_JOURNALIGNORECASE,
    XC_BLOCKMOVING,
    XC_TRACK,
    XC_SETUSEDIGNORELIST,
    XC_TERMINATE
};

const char *const SIMPLE_COMMAND_NAMES[] = {
    "info",         "infotile",   "infomenu",      "saveconfig",  "click",
    "useobject",    "getstatus",  "attack",        "helpgump",    "closeuo",
    "warmode",      "resend",     "emoteaction",   "hide",        "ignore",
    "ignorereset",  "setdressbag", "unsetdressbag", "setarm",     "unsetarm",
    "setdress",     "unsetdress", "addtype",       "removetype",  "addobject",
    "removeobject", "showjournal", "clearjournal", "journalignorecase",
    "blockmoving",  "track",      "useignorelist", "terminate",
};

JSValue jsn_simple(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    vector<string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; i++)
    {
        // numbers pass through as their script text (serials may arrive as
        // JS numbers from historical scripts)
        if (JS_IsNumber(argv[i]))
        {
            int32_t value = 0;
            JS_ToInt32(ctx, &value, argv[i]);
            args.push_back(int_to_text(value));
        }
        else
        {
            args.push_back(arg_text(ctx, argv, argc, i, ""));
        }
    }
    run_command(ctx, SIMPLE_COMMAND_NAMES[magic], args);
    return JS_UNDEFINED;
}

// [Q] Cast(nameOrIndex[, targetSerial]) - OnCommandCast / OnCommandCastTarget.
JSValue jsn_cast(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    const string target = arg_text(ctx, argv, argc, 1, "");
    if (!target.empty())
    {
        auto *command = sc_make(SC_CAST);
        command->BoolA = true; // targeted
        if (JS_IsNumber(argv[0]))
        {
            command->IntA = arg_int(ctx, argv, argc, 0, 0);
        }
        else
        {
            command->TextA = arg_text(ctx, argv, argc, 0, "");
        }
        command->TextB = target;
        ScriptQueue_Exec(command);
        delete command;
    }
    else if (JS_IsNumber(argv[0]))
    {
        auto *command = sc_make(SC_CAST);
        command->IntA = arg_int(ctx, argv, argc, 0, 0); // CPacketCastSpellRequest path
        ScriptQueue_Exec(command);
        delete command;
    }
    else
    {
        run_command(ctx, "cast", { arg_text(ctx, argv, argc, 0, "") });
    }
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] UseSkill(nameOrIndex[, targetSerial]) - HCOMMAND(useskill) /
// CSkillManager::TargetUse; the numeric form sends CPacketUseSkill directly.
JSValue jsn_useskill(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    const string target = arg_text(ctx, argv, argc, 1, "");
    auto *command = sc_make(SC_USE_SKILL);
    if (JS_IsNumber(argv[0]))
    {
        command->IntA = arg_int(ctx, argv, argc, 0, 0);
    }
    else
    {
        command->TextA = arg_text(ctx, argv, argc, 0, "");
    }
    command->TextB = target;
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [G] SkillValue(nameOrIndex[, type]) - HCOMMAND(skillsvalue) reads
// g_Skills[] on the main thread.
JSValue jsn_skillvalue(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_SKILL_VALUE);
    if (!JS_IsUndefined(argv[0]) && JS_IsNumber(argv[0]))
    {
        command->IntA = arg_int(ctx, argv, argc, 0, 0);
    }
    else
    {
        command->TextA = arg_text(ctx, argv, argc, 0, "");
    }
    command->TextB = arg_text(ctx, argv, argc, 1, "real");
    ScriptQueue_Exec(command);
    const int value = result_int(command);
    pause_gate(ctx);
    return JS_NewInt32(ctx, value);
}

// [Q] Print(color, text)/Print(text) - OnCommandPrint with the journal-side
// flood throttle.
JSValue jsn_print(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_SAY);
    command->SubTag = SAY_PRINT;
    if (argc >= 2)
    {
        command->TextA = arg_text(ctx, argv, argc, 0, "0xFFFF"); // color
        command->TextB = arg_text(ctx, argv, argc, 1, "");
    }
    else
    {
        command->TextA = "0xFFFF";
        command->TextB = arg_text(ctx, argv, argc, 0, "");
    }
    ScriptQueue_Exec(command);
    delete command;
    return JS_UNDEFINED;
}

// [Q] CharPrint(serial, color, text) - OnCommandCharPrint.
JSValue jsn_charprint(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_SAY);
    command->SubTag = SAY_CHARPRINT;
    command->TextA = arg_text(ctx, argv, argc, 0, "self");
    command->TextB = arg_text(ctx, argv, argc, 2, "");
    command->UshortA = (ushort)strtoul(arg_text(ctx, argv, argc, 1, "0xFFFF").c_str(), nullptr, 16);
    ScriptQueue_Exec(command);
    delete command;
    return JS_UNDEFINED;
}

// [Q] Say + the prefix variants (the engine composes the prefixes onto
// Say, exact strings: yell "! ", whisper "; ",
// emote ": ", broadcast "? ", party "/ ", guild "\\ ", alliance "| ").
JSValue jsn_say(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    string text = arg_text(ctx, argv, argc, 0, "");
    // the prefix table indexes XA_SAY_KIND; PRINT/CHARPRINT never arrive here
    static const char *const PREFIXES[] = {
        "", "", "", "! ", "; ", ": ", "? ", "\\ ", "| ", "/ "
    };
    if (magic != SAY_NORMAL && magic != SAY_BROADCAST && !text.empty())
    {
        text = string(PREFIXES[magic]) + text;
    }
    auto *command = sc_make(SC_SAY);
    command->SubTag = magic;
    command->TextB = text;
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] RenameMount(serial, text) - OnCommandRenameMount (packet + gate).
JSValue jsn_renamemount(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_CLIENT_CMD);
    command->IntA = 100; // executor switch: rename mount
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    command->TextB = arg_text(ctx, argv, argc, 1, "");
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] SetLight/SetWeather/SetSeason/SetTrack - the HCOMMAND bodies gate on
// the matching config filters, so the text-command path is exact.
JSValue jsn_onoff(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    static const char *const NAMES[] = { "setlight", "setweather", "setseason", "track" };
    vector<string> args;
    for (int i = 0; i < argc && i < 4; i++)
    {
        if (JS_IsNumber(argv[i]))
        {
            int32_t value = 0;
            JS_ToInt32(ctx, &value, argv[i]);
            args.push_back(int_to_text(value));
        }
        else if (JS_IsBool(argv[i]))
        {
            args.push_back(JS_ToBool(ctx, argv[i]) ? "true" : "false");
        }
        else
        {
            args.push_back(arg_text(ctx, argv, argc, i, ""));
        }
    }
    run_command(ctx, NAMES[magic], args);
    return JS_UNDEFINED;
}

// [Q] Sound(id) - HCOMMAND(sound).
JSValue jsn_sound(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    run_command(ctx, "sound", { int_to_text(arg_int(ctx, argv, argc, 0, 0)) });
    return JS_UNDEFINED;
}

// [Q] Morph([graphic]) - HCOMMAND(morph).
JSValue jsn_morph(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    run_command(ctx, "morph", argc > 0 ? vector<string>{ arg_text(ctx, argv, argc, 0, "") } : vector<string>());
    return JS_UNDEFINED;
}

// [Q] UseType/UseFromGround/UseTypeList/UseFromGroundList - [G] the found
// serial comes back (the original passed a bool* result); the find+doubleclick
// body is the OnCommandUseType family.
JSValue jsn_usetype(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_USE_TYPE);
    command->SubTag = magic;
    command->ListA = arg_list(ctx, argv, argc, 0);  // graphic | list name
    command->ListB = arg_list(ctx, argv, argc, 1);  // color (use) | flags (lists)
    command->TextA = arg_text(ctx, argv, argc, 2, ""); // container | distance
    command->TextB = arg_text(ctx, argv, argc, 3, ""); // flags (use) | extra
    command->BoolA = arg_bool(ctx, argv, argc, 4, true); // recurse
    ScriptQueue_Exec(command);
    const bool found = result_bool(command);
    pause_gate(ctx);
    return JS_NewBool(ctx, found);
}

// [G] FindType(graphic[, color[, container[, flags[, distance[, notoriety[,
// recurse]]]]]]) -> array of serial texts (OnCommandFindType body).
JSValue jsn_find(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    // magic 0 = FindType, 1 = FindList (g_Assistant.FindType/FindList)
    auto *command = sc_make(SC_FIND_TYPE);
    command->SubTag = magic;
    command->ListA = arg_list(ctx, argv, argc, 0);            // graphic
    command->ListB = arg_list(ctx, argv, argc, 1);            // color
    command->TextA = arg_text(ctx, argv, argc, 2, "self");    // container
    command->TextB = arg_text(ctx, argv, argc, 3, "");        // flags
    command->TextC = arg_text(ctx, argv, argc, 4, "");        // distance
    command->TextD = arg_text(ctx, argv, argc, 5, "");        // notoriety
    command->BoolA = arg_bool(ctx, argv, argc, 6, true);      // recurse
    ScriptQueue_Exec(command);
    vector<string> list = result_list(command);
    pause_gate(ctx);

    JSValue array = JS_NewArray(ctx);
    for (size_t i = 0; i < list.size(); i++)
    {
        JS_SetPropertyUint32(ctx, array, (uint32_t)i,
            JS_NewStringLen(ctx, list[i].c_str(), list[i].size()));
    }
    return array;
}

} // namespace

namespace { // appended natives (same unnamed namespace via nesting)
// [Q] Count(graphic[, color[, container[, distance[, recurse]]]]) ->
// g_Assistant.CountType on the main thread (the defaults:
// color 0xFFFF, container "self", distance "finddistance", recurse true).
JSValue jsn_count(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_COUNT);
    command->ListA = arg_list(ctx, argv, argc, 0);
    command->ListB = arg_list(ctx, argv, argc, 1);
    command->TextA = arg_text(ctx, argv, argc, 2, "self");
    command->TextC = arg_text(ctx, argv, argc, 3, "finddistance");
    command->BoolA = arg_bool(ctx, argv, argc, 4, true);
    ScriptQueue_Exec(command);
    const int value = result_int(command);
    pause_gate(ctx);
    return JS_NewInt32(ctx, value);
}

// [G] FindObject(serial) -> GameObject | null (OnCommandFindObject).
JSValue jsn_findobject(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_FIND_OBJECT);
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    ScriptQueue_Exec(command);
    const uint serial = result_serial(command);
    pause_gate(ctx);
    return serial != 0 ? new_game_object(ctx, serial) : JS_NULL;
}

// [G] GetDistance(serial | x, y) - OnCommandGetDistance (default 100500).
JSValue jsn_getdistance(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_GET_DISTANCE);
    if (argc >= 2)
    {
        command->IntA = arg_int(ctx, argv, argc, 0, 0);
        command->IntB = arg_int(ctx, argv, argc, 1, 0);
    }
    else
    {
        command->TextA = arg_text(ctx, argv, argc, 0, "");
    }
    ScriptQueue_Exec(command);
    const int value = result_int(command, 100500);
    pause_gate(ctx);
    return JS_NewInt32(ctx, value);
}

// [G] ObjAtLayer(layer[, serial]) -> item GameObject | null
// (OnCommandObjAtLayer; TextToLayer resolves named layers).
JSValue jsn_objatlayer(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_OBJ_AT_LAYER);
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    command->TextB = arg_text(ctx, argv, argc, 1, "");
    ScriptQueue_Exec(command);
    const uint serial = result_serial(command);
    pause_gate(ctx);
    return serial != 0 ? new_game_object(ctx, serial) : JS_NULL;
}

// [D] Now() - the only hot direct read (g_Ticks is a plain ms counter).
JSValue jsn_now(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    return JS_NewInt32(ctx, (int32_t)g_Ticks);
}

// [G] HaveTarget() - OnCommandHaveTarget: 0 = none, cursorType + 1.
JSValue jsn_havetarget(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    // the original HaveTarget was a bool; GetTargetType (jsn_targettype) is the
    // raw CommandHaveTarget value behind the same drain read
    return JS_NewBool(ctx, result_int(sc_exec(SC_HAVE_TARGET)) != 0);
}

// [Q] WaitTargetObject/Type/Ground/Tile + the immediate Target* sends: one
// tag family (XA_HOOK_KIND), the drain arms the CTarget hooks or runs the
// immediate-reply body (OnCommandWaitTarget* / OnCommandTarget*).
JSValue jsn_hook(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_WAIT_TARGET);
    command->SubTag = magic;
    command->ListA = arg_list(ctx, argv, argc, 0);   // objects | graphic | tile
    command->ListB = arg_list(ctx, argv, argc, 1);   // color | x,y,z
    command->TextA = arg_text(ctx, argv, argc, 2, "");
    command->TextB = arg_text(ctx, argv, argc, 3, "");
    command->IntA = arg_int(ctx, argv, argc, 4, 0);
    command->IntB = arg_int(ctx, argv, argc, 5, 0);
    command->IntC = arg_int(ctx, argv, argc, 6, 0);
    command->BoolA = arg_bool(ctx, argv, argc, 7, false); // relative (tile)
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] CancelWaitTarget - OnCommandCancelWaitTarget:
// drops the armed CTarget hooks through the drain (SC_CANCEL_WAIT_TARGET).
JSValue jsn_cancelwaittarget(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    auto *command = sc_make(SC_CANCEL_WAIT_TARGET);
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

namespace {

// [Q] WaitMenu(prompt, choice) - the original body armed the menu manager's
// wait hook (SetHook); the manager auto-sends the choice
// when the named menu arrives. Fire-and-forget
// + the pause gate (the WaitForMenu flag loop is the polling half).
JSValue jsn_waitmenu(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_WAIT_MENU);
    command->TextA = arg_text(ctx, argv, argc, 0, ""); // prompt
    command->TextB = arg_text(ctx, argv, argc, 1, ""); // choice
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] CancelTarget - OnCommandCancelTarget (4521): cancel the client target
// cursor when it is up
JSValue jsn_canceltarget(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    auto *command = sc_make(SC_CLIENT_CMD);
    command->IntA = 205;
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] CancelWaitMenu - OnCommandCancelWaitMenu: ClearHooks.
JSValue jsn_cancelwaitmenu(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    auto *command = sc_make(SC_WAIT_MENU);
    command->BoolA = true; // cancel
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [G] MenuCount/GetMenu/SelectMenu/CloseMenu (OnCommandMenuCount 3554,
// OnCommandGetMenu 3564 - "last"/"lastmenu" resolve to the last parsed menu,
// OnCommandSelectMenu/CloseMenu 3581/3592 through SendChoiceIfMenuExists).
JSValue jsn_menucount(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    auto *command = sc_make(SC_MENU_INFO);
    command->SubTag = 0;
    ScriptQueue_Exec(command);
    return JS_NewInt32(ctx, result_int(command));
}

JSValue jsn_getmenu(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_MENU_INFO);
    command->SubTag = 1;
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    ScriptQueue_Exec(command);
    CMenu *menu =
        command->ResultDropped ? nullptr : (CMenu *)command->PointerA;
    delete command;
    pause_gate(ctx);
    if (menu == nullptr)
    {
        // the original handed the wrapper a null menu: the object read as empty
        // (Serial "0x00000000", empty name) instead of null
        menu = new CMenu();
    }
    JSValue object = new_menu_object(ctx, menu);
    return object;
}

JSValue jsn_selectmenu(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_MENU_SELECT);
    command->TextA = arg_text(ctx, argv, argc, 0, ""); // menu name
    command->TextB = arg_text(ctx, argv, argc, 1, ""); // item name
    ScriptQueue_Exec(command);
    bool result = result_bool(command);
    delete command;
    pause_gate(ctx);
    return JS_NewBool(ctx, result);
}

JSValue jsn_closemenu(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_MENU_SELECT);
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    command->BoolA = true; // close: empty item name (SendChoiceIfMenuExists(""))
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [G] the journal query family: InJournal(...)-> entry | null,
// WaitJournal(pattern, start[, end[, flags[, serial[, color]]]]) loops with
// the pause/terminate checks (CScriptEngine::WaitJournal).
JSValue jsn_injournal(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_JOURNAL_FIND);
    command->TextA = arg_text(ctx, argv, argc, 0, "");    // pattern
    command->TextB = arg_text(ctx, argv, argc, 1, "");    // flags (my|sys|...)
    command->TextC = arg_text(ctx, argv, argc, 2, "0");   // serial
    command->TextD = arg_text(ctx, argv, argc, 3, "0xFFFF"); // color
    command->IntA = 0;                                    // startTime
    command->IntB = 0;                                    // endTime
    command->BoolA = argc >= 5 && arg_bool(ctx, argv, argc, 5, false); // consume
    ScriptQueue_Exec(command);
    CJournalMessage *message =
        command->ResultDropped ? nullptr : (CJournalMessage *)command->PointerA;
    delete command;
    pause_gate(ctx);
    if (message == nullptr)
    {
        return JS_NULL;
    }
    JSValue entry = new_journal_entry(ctx, *message);
    delete message;
    return entry;
}

JSValue jsn_waitjournal(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    const string pattern = arg_text(ctx, argv, argc, 0, "");
    const string flagsText = arg_text(ctx, argv, argc, 1, "");
    const string serialText = arg_text(ctx, argv, argc, 2, "0");
    const string colorText = arg_text(ctx, argv, argc, 3, "0xFFFF");
    const uint startTime = (uint)arg_int(ctx, argv, argc, 4, 0);
    uint endTime = (uint)arg_int(ctx, argv, argc, 5, 0);

    CScriptEngine *engine = (CScriptEngine *)JS_GetContextOpaque(ctx);
    while ((endTime == 0u) || g_Ticks < endTime)
    {
        if (engine != nullptr && engine->GetTerminated())
        {
            return JS_NULL;
        }
        auto *command = sc_make(SC_JOURNAL_FIND);
        command->TextA = pattern;
        command->TextB = flagsText;
        command->TextC = serialText;
        command->TextD = colorText;
        command->IntA = (int)startTime;
        command->IntB = (int)endTime;
        ScriptQueue_Exec(command);
        CJournalMessage *message =
            command->ResultDropped ? nullptr : (CJournalMessage *)command->PointerA;
        delete command;

        if (message != nullptr)
        {
            JSValue entry = new_journal_entry(ctx, *message);
            delete message;
            return entry;
        }

        // 10 ms poll, then the pause gate (the original loop's msleep + PauseWaiting)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        pause_gate(ctx);
    }
    return JS_NULL;
}

// [G] JournalCount/JournalLine/LastJournalMessage (OnCommandJournalCount etc.,
// reading the same ring on the main thread).
JSValue jsn_journalline(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_JOURNAL_INFO);
    command->SubTag = magic;
    command->IntA = arg_int(ctx, argv, argc, 0, 0);
    command->BoolA = arg_bool(ctx, argv, argc, 1, false); // consume (remove)
    ScriptQueue_Exec(command);
    if (command->ResultDropped || command->PointerA == nullptr)
    {
        delete command;
        return JS_NULL;
    }
    CJournalMessage *message = (CJournalMessage *)command->PointerA;
    JSValue entry = new_journal_entry(ctx, *message);
    delete message;
    delete command;
    return entry;
}

// [G] Journal entry accessors (CScriptJournalObject methods).
// [G] JournalCount - OnCommandJournalCount over the mirror ring
JSValue jsn_journalcount(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    auto *command = sc_make(SC_JOURNAL_INFO);
    command->SubTag = 3; // count
    ScriptQueue_Exec(command);
    return JS_NewInt32(ctx, result_int(command));
}

// [G] ObjectExists(serial) - OnCommandObjectExists: the Exists field read
JSValue jsn_objectexists(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_OBJECT_GET);
    command->SerialA = CAssistant::TextToSerial(arg_text(ctx, argv, argc, 0, ""));
    command->SubTag = OF_BOOL_EXISTS;
    ScriptQueue_Exec(command);
    return JS_NewBool(ctx, result_bool(command));
}

// [Q] GetSerial/GetGraphic/GetContainer - OnCommandGetSerial/GetGraphic/
// GetContainer (3491/3736/3746): pure serial/graphic text normalization
JSValue jsn_getserialtext(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_CLIENT_CMD);
    command->IntA = magic; // 400 serial / 401 graphic / 402 container
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    ScriptQueue_Exec(command);
    return JS_NewStringLen(
        ctx, command->ResultText.c_str(), command->ResultText.size());
}

JSValue journal_get(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic)
{
    (void)argc;
    (void)argv;
    auto *data = (XA_JS_JOURNALENTRY *)JS_GetOpaque2(ctx, thisVal, s_JournalClass);
    if (data == nullptr || data->Message == nullptr)
    {
        return JS_EXCEPTION;
    }
    const CJournalMessage *message = data->Message;
    switch (magic)
    {
        case 0: // Text()
            return JS_NewStringLen(
                ctx, message->GetText().c_str(), message->GetText().size());
        case 1: // Color()
            return JS_NewInt32(ctx, message->GetColor());
        case 2: // Timer()
            return JS_NewInt32(ctx, (int32_t)message->GetTimer());
        case 3: // Flags()
            return JS_NewInt32(ctx, message->GetFlags());
        case 4: // Serial()
            return JS_NewStringLen(ctx, CAssistant::SerialToText(message->GetSerial()).c_str(),
                CAssistant::SerialToText(message->GetSerial()).size());
        case 5: // FindTextID()
            return JS_NewInt32(ctx, message->GetFindTextID());
        default:
            return JS_UNDEFINED;
    }
}

// [D] Text()/local SetText(newText) - CScriptJournalObject::SetText kept the
// text on the local copy only (the original object mutated its own m_Message).
JSValue journal_settext(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv)
{
    auto *data = (XA_JS_JOURNALENTRY *)JS_GetOpaque2(ctx, thisVal, s_JournalClass);
    if (data == nullptr || data->Message == nullptr)
    {
        return JS_EXCEPTION;
    }
    data->Message->SetText(arg_text(ctx, argv, argc, 0, ""));
    return JS_UNDEFINED;
}

// [Q] movement: CanWalk(direction, x, y, z) / Step(direction[, run]) /
// WalkTo(x, y, z[, distance]) / IsWalking() - the OnCommand* bodies call
// g_PathFinder with the EnabledCommandMoving gate.
JSValue jsn_move(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_CLIENT_CMD);
    // 200=canwalk 201=step 202=walkto 203=iswalking (the executor switch);
    // the int fields carry (direction|x, run-flag-slot|y, y|z, z) per kind.
    command->IntA = 200 + magic;
    command->IntB = arg_int(ctx, argv, argc, 0, 0);
    command->IntC = arg_int(ctx, argv, argc, 1, 0);
    command->SerialA = (uint)arg_int(ctx, argv, argc, 2, 0);
    command->UshortA = (ushort)arg_int(ctx, argv, argc, 3, 0);
    command->BoolA = arg_bool(ctx, argv, argc, magic == 1 ? 1 : 4, false); // Step's run flag
    ScriptQueue_Exec(command);
    const bool value = result_bool(command);
    pause_gate(ctx);
    return JS_NewBool(ctx, value);
}

// [Q] StopWalking - OnCommandStopWalking.
JSValue jsn_stopwalking(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    auto *command = sc_make(SC_CLIENT_CMD);
    command->IntA = 204; // executor switch: stop walking
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] the dress/arm family (OnCommandSetDressBag..OnCommandEquipT): one tag,
// the drain dispatches on XA_DRESS_KIND (dress ops run the config + dress
// agent bodies the text commands use).
JSValue jsn_dress(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_DRESS_OPS);
    command->SubTag = magic;
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    command->TextB = arg_text(ctx, argv, argc, 1, "");
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] MoveItem/Drop/DropHere - the HCOMMAND bodies (defaults: count=all,
// container=backpack, ground coords) run unchanged on the main thread.
JSValue jsn_moveitem(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    vector<string> args;
    for (int i = 0; i < argc; i++)
    {
        if (JS_IsNumber(argv[i]))
        {
            double number = 0;
            JS_ToFloat64(ctx, &number, argv[i]);
            char buf[32] = { 0 };
            const double whole = (double)(long long)number;
            if (number > -1e15 && number < 1e15 && (number - whole) < 0.5 &&
                (whole - number) <= 0.5)
            {
                snprintf(buf, sizeof(buf), "%lld", (long long)number);
            }
            else
            {
                snprintf(buf, sizeof(buf), "%f", number);
            }
            args.push_back(buf);
        }
        else
        {
            args.push_back(arg_text(ctx, argv, argc, i, ""));
        }
    }
    static const char *const NAMES[] = { "moveitem", "drop", "drophere" };
    run_command(ctx, NAMES[magic], args);
    return JS_UNDEFINED;
}

// [Q] LoadScript(path) - CFileEngine::LoadScript read the file into the
// editor (the tab's LoadScript). The buffer write MUST happen on the main
// thread (the worker only marshals: SC_EDITOR_LOAD executes
// ScriptManager_LoadScriptFile in the drain and the worker just waits).
JSValue jsn_loadscript(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_EDITOR_LOAD);
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] Exec(name[, once[, args]]) - OnCommandExec: start a named function of
// the current script text as its own running script.
JSValue jsn_exec(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_TERMINATE_SCRIPT);
    command->SubTag = 2; // executor switch: exec
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    command->BoolA = arg_bool(ctx, argv, argc, 1, false);
    command->ListA = arg_list(ctx, argv, argc, 2);
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] Terminate(name[, save]) goes through the `terminate` text command
// (jsn_simple XC_TERMINATE, HCOMMAND(terminate) in textcommandmanager.cpp).

// [G] ScriptRunning(name) - OnCommandScriptRunning.
JSValue jsn_scriptrunning(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_TERMINATE_SCRIPT);
    command->SubTag = 1; // executor switch: running?
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    ScriptQueue_Exec(command);
    const bool value = result_bool(command);
    return JS_NewBool(ctx, value);
}

// [D] pure helpers of the engine (Time/Date/Random).
JSValue jsn_time(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    char buf[16] = { 0 };
    time_t now = time(nullptr);
    struct tm local = {};
    localtime_r(&now, &local); // thread-safe: script workers call this
    strftime(buf, sizeof(buf), "%H:%M:%S", &local);
    return JS_NewString(ctx, buf);
}

JSValue jsn_date(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    char buf[16] = { 0 };
    time_t now = time(nullptr);
    struct tm local = {};
    localtime_r(&now, &local);
    strftime(buf, sizeof(buf), "%d.%m.%Y", &local);
    return JS_NewString(ctx, buf);
}

JSValue jsn_random(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    static std::mt19937 generator((unsigned)g_Ticks);
    const int minValue = arg_int(ctx, argv, argc, 0, 0);
    const int maxValue = arg_int(ctx, argv, argc, 1, 0);
    if (maxValue == minValue)
    {
        return JS_NewInt32(ctx, minValue);
    }
    std::uniform_int_distribution<int> distribution(
        minValue < maxValue ? minValue : maxValue, minValue < maxValue ? maxValue : minValue);
    return JS_NewInt32(ctx, distribution(generator));
}

// [D] AssistVersion - the build revision string.
JSValue jsn_assistversion(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    return JS_NewString(ctx, PRODUCT_VERSION_STR);
}

// [Q] Connected() - the packetmanager connection state.
JSValue jsn_connected(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    return JS_NewBool(ctx, result_bool(sc_exec(SC_CONNECTED)));
}

// [G] ClientLastTarget/ClientLastAttack/TargetSystemSerial - the assistant
// last-target globals (queued reads).
JSValue jsn_lastserial(JSContext *ctx, JSValueConst, int, JSValueConst *, int magic)
{
    auto *command = sc_make(SC_TARGET_SYSTEM_SERIAL);
    command->SubTag = magic; // 0=lasttarget 1=lastattack 2=target system serial
    ScriptQueue_Exec(command);
    const uint serial = result_serial(command);
    return JS_NewStringLen(ctx, CAssistant::SerialToText(serial).c_str(),
        CAssistant::SerialToText(serial).size());
}

// [G] GetLastTargetPosition/GetLastAttackPosition -> {x, y}.
JSValue jsn_lastpos(JSContext *ctx, JSValueConst, int, JSValueConst *, int magic)
{
    auto *command = sc_make(SC_LAST_TARGET_POS);
    command->SubTag = magic;
    ScriptQueue_Exec(command);
    if (command->ResultDropped)
    {
        delete command;
        return JS_NULL;
    }
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, command->ResultInt));
    JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, command->IntA));
    delete command;
    return obj;
}

// [G] BuffExists(name) - OnCommandBuffExists:
// g_BuffManager.Exists over the trimmed+lowered name; the port reads the
// buff mirror (scriptstate.cpp) fed by the OnBuffAdd/Remove events.
JSValue jsn_buffexists(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_BUFF_EXISTS);
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    ScriptQueue_Exec(command);
    return JS_NewBool(ctx, result_bool(command));
}

// [Q] the engine-class families of 4b-2 (the notice stub of batch 1 is gone;
// PlayWav below is the only remaining documented deviation).

// --- the WaitFor* flag family -----------------------------------------------
// The original cleared the g_*Received global on the script thread and msleep-polled
// it; the port clears+polls it inside the drain (SC_EVENT_WAIT clears on the
// first call - BoolA false - and only reads afterwards), so the main-thread
// flags are never touched cross-thread. 1000ms default (the other overload).

enum XA_WAIT_FLAG
{
    WF_TARGET = 0,
    WF_MENU,
    WF_GUMP,
    WF_PROMPT,
    WF_SHOP,
    WF_TRADE,
    WF_CONTEXTMENU
};

JSValue jsn_waitfor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    CScriptEngine *engine = (CScriptEngine *)JS_GetContextOpaque(ctx);
    const uint maxDelay = (uint)(argc >= 1 ? arg_int(ctx, argv, argc, 0, 1000) : 1000);
    const uint deadline = g_Ticks + maxDelay;

    // the immediate pre-check: WaitForTarget
    // returned true at once when a target cursor was already up. Queued:
    // g_Target is main-thread state (SC_HAVE_TARGET reads IsTargeting()).
    if (magic == WF_TARGET)
    {
        auto *targeting = sc_make(SC_HAVE_TARGET);
        ScriptQueue_Exec(targeting);
        bool up = targeting->ResultInt != 0 && !targeting->ResultDropped;
        delete targeting;
        if (up)
        {
            return JS_TRUE;
        }
    }

    auto *command = sc_make(SC_EVENT_WAIT);
    command->SubTag = magic;
    bool armed = false; // the caller-side marker: the first call clears the
    while (true)        // flag, later calls only read it (the reset+poll)
    {
        if (engine != nullptr && engine->GetTerminated())
        {
            delete command;
            return JS_FALSE; // terminate beats the wait (plan rule)
        }
        command->BoolA = armed;
        command->ResultBool = false;
        ScriptQueue_Exec(command, 5000);
        armed = true;
        if (command->ResultDropped)
        {
            break; // shutdown: fall through with false
        }
        if (command->ResultBool)
        {
            delete command;
            pause_gate(ctx);
            return JS_TRUE;
        }
        // the original timeout shape: the loop ran
        // `while (!received && timer > elapsed())` - exit false once the
        // maxDelay budget is spent (the missing deadline made a lost event
        // hang the script forever)
        if (g_Ticks >= deadline)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    delete command;
    pause_gate(ctx);
    return JS_FALSE;
}

// --- the gump engine (gumpengine.cpp + OnCommandWaitGump 4204 etc.) ---------
// CreateGumpHook(name|index) builds the reply hook (a script-owned record);
// WaitGump arms it on the manager (the manager auto-replies the matching
// gump); GumpCount/GetGump read the parsed server gumps as COPIES.

XA_JS_GUMPHOOK *gumphook_of(JSContext *ctx, JSValueConst value)
{
    return (XA_JS_GUMPHOOK *)JS_GetOpaque2(ctx, value, s_GumpHookClass);
}

// flattens the hook into the command record (ListA = entry index/text pairs,
// ListB = check index/state pairs)
void serialize_gumphook(XA_SCRIPT_COMMAND *command, const XA_JS_GUMPHOOK *hook)
{
    command->IntA = hook->Index;
    for (const auto &entry : hook->Entries)
    {
        command->ListA.push_back(int_to_text(entry.first));
        command->ListA.push_back(entry.second);
    }
    for (const auto &check : hook->Checks)
    {
        command->ListB.push_back(int_to_text(check.first));
        command->ListB.push_back(check.second ? "1" : "0");
    }
}

JSValue jsn_creategumphook(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    // the original trimmed + lowered the text form; "cancel" meant hook index 0
    string name = arg_text(ctx, argv, argc, 0, "");
    int index = 0;
    if (JS_IsNumber(argv[0]))
    {
        int32_t value = 0;
        JS_ToInt32(ctx, &value, argv[0]);
        index = value;
    }
    else
    {
        for (char &c : name)
        {
            c = (char)tolower((unsigned char)c);
        }
        index = name == "cancel" ? 0 : atoi(name.c_str());
    }
    pause_gate(ctx);
    auto *hook = new XA_JS_GUMPHOOK();
    if (hook == nullptr)
    {
        return JS_EXCEPTION;
    }
    hook->Index = index;
    JSValue object = JS_NewObjectClass(ctx, s_GumpHookClass);
    JS_SetOpaque(object, hook);
    return object;
}

JSValue jsn_waitgump(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    // OnCommandWaitGump: g_GumpManager.AddHook(hook) - the hook rides the
    // queue as flattened lists (no main-thread pointers cross threads)
    if (argc >= 1 && JS_IsObject(argv[0]))
    {
        auto *hook = gumphook_of(ctx, argv[0]);
        if (hook != nullptr)
        {
            auto *command = sc_make(SC_GUMP_HOOK_ARM);
            serialize_gumphook(command, hook);
            ScriptQueue_Exec(command);
            delete command;
        }
    }
    pause_gate(ctx);
    return JS_UNDEFINED;
}

JSValue jsn_cancelwaitgump(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    auto *command = sc_make(SC_GUMP_HOOK_ARM);
    command->BoolA = true; // ClearHooks
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

JSValue jsn_gumpcount(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    auto *command = sc_make(SC_GUMP_INFO);
    command->SubTag = 0;
    ScriptQueue_Exec(command);
    return JS_NewInt32(ctx, result_int(command));
}

// [Q] InfoGump(index|'last') - OnCommandInfoGump: the manager's gump dump
JSValue jsn_infogump(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_GUMP_INFO);
    command->SubTag = magic;
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

JSValue jsn_getgump(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_GUMP_INFO);
    if (argc >= 2)
    {
        command->SubTag = 2; // by serial + id
        int32_t serial = 0;
        int32_t id = 0;
        JS_ToInt32(ctx, &serial, argv[0]);
        JS_ToInt32(ctx, &id, argv[1]);
        command->SerialA = (uint)serial;
        command->IntB = id;
    }
    else
    {
        command->SubTag = 1; // by index; -1 = last (GetLastGump rode the same path)
        command->IntA = arg_int(ctx, argv, argc, 0, -1);
    }
    ScriptQueue_Exec(command);
    CGump *gump = command->ResultDropped ? nullptr : (CGump *)command->PointerA;
    delete command;
    pause_gate(ctx);
    if (gump == nullptr)
    {
        return JS_NULL;
    }
    JSValue object = new_gump_object(ctx, gump); // the object owns the copy
    return object;
}

JSValue gumphook_get(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *)
{
    auto *hook = gumphook_of(ctx, thisVal);
    return hook != nullptr ? JS_NewInt32(ctx, hook->Index) : JS_EXCEPTION;
}

JSValue gumphook_addentry(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv)
{
    auto *hook = gumphook_of(ctx, thisVal);
    if (hook == nullptr)
    {
        return JS_EXCEPTION;
    }
    hook->Entries.push_back(
        std::make_pair(arg_int(ctx, argv, argc, 0, 0), arg_text(ctx, argv, argc, 1, "")));
    return JS_UNDEFINED;
}

JSValue gumphook_addcheck(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv)
{
    auto *hook = gumphook_of(ctx, thisVal);
    if (hook == nullptr)
    {
        return JS_EXCEPTION;
    }
    hook->Checks.push_back(
        std::make_pair(arg_int(ctx, argv, argc, 0, 0), arg_bool(ctx, argv, argc, 1, false)));
    return JS_UNDEFINED;
}

// the CScriptGumpObject surface over the script-owned copy
JSValue gump_get(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic)
{
    CGump *gump = (CGump *)JS_GetOpaque2(ctx, thisVal, s_GumpClass);
    if (gump == nullptr)
    {
        return JS_EXCEPTION;
    }
    switch (magic)
    {
        case 0: return JS_NewInt32(ctx, (int32_t)gump->GetSerial());
        case 1: return JS_NewInt32(ctx, (int32_t)gump->GetID());
        case 2: return JS_NewInt32(ctx, gump->GetX());
        case 3: return JS_NewInt32(ctx, gump->GetY());
        case 4: return JS_NewBool(ctx, gump->GetReplayed());
        default: return JS_NewInt32(ctx, (int32_t)gump->GetReplyID());
    }
}

// filters the item commands by control type (the ButtonList..CommandList
// bodies of gumpengine.cpp)
vector<string> gump_commands_of(const CGump *gump, int filter)
{
    vector<string> result;
    for (const CGumpItem &item : gump->m_Items)
    {
        switch (filter)
        {
            case 0: // buttons
                if (item.GetType() != GIT_BUTTON && item.GetType() != GIT_BUTTONTILEART)
                {
                    continue;
                }
                break;
            case 1: // checkboxes
                if (item.GetType() != GIT_CHECKBOX)
                {
                    continue;
                }
                break;
            case 2: // radios
                if (item.GetType() != GIT_RADIO)
                {
                    continue;
                }
                break;
            case 3: // tilepics
                if (item.GetType() != GIT_TILEPIC && item.GetType() != GIT_TILEPICHUE)
                {
                    continue;
                }
                break;
            case 4: // gumppics
                if (item.GetType() != GIT_GUMPPIC && item.GetType() != GIT_GUMPPICTILED)
                {
                    continue;
                }
                break;
            case 5: // text entries
                if (item.GetType() != GIT_TEXTENTRY && item.GetType() != GIT_TEXTENTRYLIMITED)
                {
                    continue;
                }
                break;
            default: // the whole command list
                break;
        }
        result.push_back(item.GetCommand());
    }
    return result;
}

JSValue gump_list(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic)
{
    CGump *gump = (CGump *)JS_GetOpaque2(ctx, thisVal, s_GumpClass);
    if (gump == nullptr)
    {
        return JS_EXCEPTION;
    }
    if (magic == 7) // TextList: the gump text lines
    {
        JSValue array = JS_NewArray(ctx);
        for (size_t i = 0; i < gump->m_Text.size(); i++)
        {
            JS_SetPropertyUint32(ctx, array, (uint32_t)i,
                JS_NewStringLen(ctx, gump->m_Text[i].c_str(), gump->m_Text[i].size()));
        }
        return array;
    }
    const vector<string> lines = gump_commands_of(gump, magic);
    JSValue array = JS_NewArray(ctx);
    for (size_t i = 0; i < lines.size(); i++)
    {
        JS_SetPropertyUint32(ctx, array, (uint32_t)i,
            JS_NewStringLen(ctx, lines[i].c_str(), lines[i].size()));
    }
    return array;
}

JSValue gump_command(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv)
{
    CGump *gump = (CGump *)JS_GetOpaque2(ctx, thisVal, s_GumpClass);
    if (gump == nullptr)
    {
        return JS_EXCEPTION;
    }
    const int index = arg_int(ctx, argv, argc, 0, 0);
    if (index >= 0 && index < (int)gump->m_Items.size())
    {
        const string &command = gump->m_Items[index].GetCommand();
        return JS_NewStringLen(ctx, command.c_str(), command.size());
    }
    return JS_NewString(ctx, "");
}

JSValue gump_text(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv)
{
    CGump *gump = (CGump *)JS_GetOpaque2(ctx, thisVal, s_GumpClass);
    if (gump == nullptr)
    {
        return JS_EXCEPTION;
    }
    const int index = arg_int(ctx, argv, argc, 0, 0);
    if (index >= 0 && index < (int)gump->m_Text.size())
    {
        return JS_NewStringLen(
            ctx, gump->m_Text[index].c_str(), gump->m_Text[index].size());
    }
    return JS_NewString(ctx, "");
}

// Gump.Select(hook) / Gump.Close - OnCommandSelectGump through
// g_GumpManager.SendGumpChoice. The queue contract forbids passing the live
// manager pointer across threads (the original body passed m_Parent raw), so the
// copy's serial+id re-resolve the live gump inside the drain - same reply
// packet, no dangling window.
JSValue gump_select(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv)
{
    CGump *gump = (CGump *)JS_GetOpaque2(ctx, thisVal, s_GumpClass);
    if (gump == nullptr)
    {
        return JS_EXCEPTION;
    }
    auto *command = sc_make(SC_GUMP_SELECT);
    command->TextA = int_to_text((int)gump->GetSerial());
    command->TextB = int_to_text((int)gump->GetID());
    if (argc >= 1 && JS_IsObject(argv[0]))
    {
        auto *hook = gumphook_of(ctx, argv[0]);
        if (hook != nullptr)
        {
            serialize_gumphook(command, hook);
        }
    }
    ScriptQueue_Exec(command);
    bool result = result_bool(command);
    delete command;
    pause_gate(ctx);
    return JS_NewBool(ctx, result);
}

JSValue gump_close(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *)
{
    // the original Close built an empty hook with index 0 (the "cancel" reply)
    return gump_select(ctx, thisVal, 0, nullptr);
}

// --- the menu object (menuengine.cpp) ----------------------------------------

JSValue menu_get(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic)
{
    CMenu *menu = (CMenu *)JS_GetOpaque2(ctx, thisVal, s_MenuClass);
    if (menu == nullptr)
    {
        return JS_EXCEPTION;
    }
    switch (magic)
    {
        case 0: // Serial -> serial text
        {
            const string text = CAssistant::SerialToText(menu->GetSerial());
            return JS_NewStringLen(ctx, text.c_str(), text.size());
        }
        case 1: // ID -> graphic text
        {
            const string text = CAssistant::GraphicToText(menu->GetID());
            return JS_NewStringLen(ctx, text.c_str(), text.size());
        }
        case 2: // Name
            return JS_NewStringLen(ctx, menu->GetName().c_str(), menu->GetName().size());
        case 3: // IsGrayMenu
            return JS_NewBool(ctx, menu->GetGrayMenu());
        case 4: // ItemsCount
            return JS_NewInt32(ctx, (int32_t)menu->m_Items.size());
        default: // the item accessors (index argument)
        {
            const int index = arg_int(ctx, argv, argc, 0, 0);
            if (index < 0 || index >= (int)menu->m_Items.size())
            {
                // the original accessors returned 0 / "" out of range
                return magic == 5 ? JS_NewInt32(ctx, 0) : JS_NewString(ctx, "");
            }
            const CMenuItem &item = menu->m_Items[index];
            switch (magic)
            {
                case 5: // ItemID
                    return JS_NewInt32(ctx, item.GetID());
                case 6: // ItemGraphic
                {
                    const string text = CAssistant::GraphicToText(item.GetGraphic());
                    return JS_NewStringLen(ctx, text.c_str(), text.size());
                }
                case 7: // ItemColor
                {
                    const string text = CAssistant::GraphicToText(item.GetColor());
                    return JS_NewStringLen(ctx, text.c_str(), text.size());
                }
                default: // ItemName
                    return JS_NewStringLen(
                        ctx, item.GetName().c_str(), item.GetName().size());
            }
        }
    }
}

// Select(index|name) - the int form resolved the item name locally then sent
// the same CommandSelectMenu pair (menuengine.cpp)
JSValue menu_select(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv)
{
    CMenu *menu = (CMenu *)JS_GetOpaque2(ctx, thisVal, s_MenuClass);
    if (menu == nullptr)
    {
        return JS_EXCEPTION;
    }
    string itemName;
    if (JS_IsNumber(argv[0]))
    {
        const int index = arg_int(ctx, argv, argc, 0, -1);
        if (index < 0 || index >= (int)menu->m_Items.size())
        {
            pause_gate(ctx);
            return JS_FALSE; // the int form no-op'd out of range
        }
        itemName = menu->m_Items[index].GetName();
    }
    else
    {
        itemName = arg_text(ctx, argv, argc, 0, "");
    }

    auto *command = sc_make(SC_MENU_SELECT);
    command->TextA = menu->GetName();
    command->TextB = itemName;
    ScriptQueue_Exec(command);
    bool result = result_bool(command);
    delete command;
    pause_gate(ctx);
    return JS_NewBool(ctx, result);
}

JSValue menu_close(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *)
{
    CMenu *menu = (CMenu *)JS_GetOpaque2(ctx, thisVal, s_MenuClass);
    if (menu == nullptr)
    {
        return JS_EXCEPTION;
    }
    auto *command = sc_make(SC_MENU_SELECT);
    command->TextA = menu->GetName();
    command->BoolA = true; // close
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// --- prompt (WaitPrompt 3272 / PromptExists 3451 / SendPrompt 3484) ----------

JSValue jsn_waitprompt(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_PROMPT_HOOK);
    command->TextA = arg_text(ctx, argv, argc, 0, "");       // reply text
    command->TextB = arg_text(ctx, argv, argc, 1, "0");      // serial
    const string type = arg_text(ctx, argv, argc, 2, "all"); // all|ascii|unicode
    command->IntA = type == "ascii" ? PT_ASCII : (type == "unicode" ? PT_UNICODE : PT_NONE);
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

JSValue jsn_cancelwaitprompt(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    auto *command = sc_make(SC_PROMPT_HOOK);
    command->BoolA = true; // ClearHooks
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

JSValue jsn_promptinfo(JSContext *ctx, JSValueConst, int, JSValueConst *, int magic)
{
    auto *command = sc_make(SC_PROMPT_INFO);
    command->SubTag = magic; // 0 = exists, 1 = serial, 2 = id
    ScriptQueue_Exec(command);
    if (magic == 0)
    {
        return JS_NewBool(ctx, result_bool(command));
    }
    const string text = CAssistant::SerialToText(command->ResultSerial);
    return JS_NewStringLen(ctx, text.c_str(), text.size());
}

JSValue jsn_sendprompt(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_PROMPT_SEND);
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// --- trade (OnCommandTrade* 3607-3734) ----------------------------------------

JSValue jsn_tradeinfo(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_TRADE_INFO);
    command->SubTag = magic;
    command->TextA = arg_text(ctx, argv, argc, 0, ""); // trade index
    if (magic == 1 || magic == 4)                      // container/checkstate
    {
        // the "right" container text meant opponent 1 (the original bodies)
        const string container = arg_text(ctx, argv, argc, 1, "0");
        string lowered = container;
        for (char &c : lowered)
        {
            c = (char)tolower((unsigned char)c);
        }
        command->IntA = lowered == "right" ? 1 : atoi(container.c_str());
    }
    ScriptQueue_Exec(command);
    if (magic == 0)
    {
        return JS_NewInt32(ctx, result_int(command));
    }
    if (magic == 4)
    {
        return JS_NewBool(ctx, result_bool(command));
    }
    return JS_NewStringLen(
        ctx, command->ResultText.c_str(), command->ResultText.size());
}

JSValue jsn_tradecmd(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_TRADE_CMD);
    command->SubTag = magic; // 0 = check, 1 = close
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    command->BoolA = arg_bool(ctx, argv, argc, 1, false);
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// --- shop (Buy 3296 / Sell 3320: CommandShop + the IsShopping wait loop) -----

JSValue jsn_shop(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    CScriptEngine *engine = (CScriptEngine *)JS_GetContextOpaque(ctx);
    auto *command = sc_make(SC_SHOP);
    command->TextA = arg_text(ctx, argv, argc, 0, "");               // shop list
    command->TextB = argc >= 2 ? arg_text(ctx, argv, argc, 1, "") : string(); // vendor
    command->BoolA = magic == 0;                                     // buy
    ScriptQueue_Exec(command);
    delete command;

    // the original loop polled IsShopping every 10 ms; the terminate check is the
    // Phase-3 addition (it had none and outlived Terminate)
    while (engine == nullptr || !engine->GetTerminated())
    {
        auto *state = sc_make(SC_SHOP_STATE);
        ScriptQueue_Exec(state);
        bool shopping = !state->ResultDropped && state->ResultBool;
        delete state;
        if (!shopping)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        pause_gate(ctx);
    }
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// --- context menu (OnCommandRequest/Wait/CancelContextMenu 4605+) -------------

JSValue jsn_contextmenu(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_CONTEXT_MENU);
    command->SubTag = magic;
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    command->IntA = arg_int(ctx, argv, argc, 1, 0);
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// --- timers (OnCommandTimer* 4462-4519; the call sites lowercased names) ------

JSValue jsn_timer(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_TIMER);
    command->SubTag = magic;
    string name = arg_text(ctx, argv, argc, 0, "");
    for (char &c : name)
    {
        c = (char)tolower((unsigned char)c);
    }
    command->TextA = name;
    command->IntA = arg_int(ctx, argv, argc, 1, 0);
    ScriptQueue_Exec(command);
    if (magic == 0)
    {
        return JS_NewBool(ctx, result_bool(command));
    }
    if (magic == 2)
    {
        return JS_NewInt32(ctx, result_int(command));
    }
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] SetGlobal(name, value) / ClearGlobals - the command-manager map
// (assistant-global: shared by every script, session lifetime)
JSValue jsn_globalset(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_GLOBAL_SET);
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    command->TextB = arg_text(ctx, argv, argc, 1, "");
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

JSValue jsn_globalclear(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    auto *command = sc_make(SC_GLOBAL_SET);
    command->SubTag = 1; // clear
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] CreateClientMacro - the CClientMacroEngine object: the action lists
// live on the script side, Play marshals them into CPacketPlayMacro.
JSValue jsn_createclientmacro(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    pause_gate(ctx);
    auto *macro = new XA_JS_CLIENTMACRO();
    if (macro == nullptr)
    {
        return JS_EXCEPTION;
    }
    if (argc >= 1)
    {
        macro->Actions.push_back(arg_text(ctx, argv, argc, 0, ""));
        macro->SubActions.push_back(argc >= 2 ? arg_text(ctx, argv, argc, 1, "") : string());
    }
    JSValue object = JS_NewObjectClass(ctx, s_ClientMacroClass);
    JS_SetOpaque(object, macro);
    return object;
}

JSValue macro_addaction(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv)
{
    auto *macro = (XA_JS_CLIENTMACRO *)JS_GetOpaque2(ctx, thisVal, s_ClientMacroClass);
    if (macro == nullptr)
    {
        return JS_EXCEPTION;
    }
    macro->Actions.push_back(arg_text(ctx, argv, argc, 0, ""));
    macro->SubActions.push_back(argc >= 2 ? arg_text(ctx, argv, argc, 1, "") : string());
    return JS_UNDEFINED;
}

JSValue macro_play(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv)
{
    auto *macro = (XA_JS_CLIENTMACRO *)JS_GetOpaque2(ctx, thisVal, s_ClientMacroClass);
    if (macro == nullptr || macro->Actions.empty())
    {
        return JS_UNDEFINED;
    }
    const bool wait = argc >= 1 ? arg_bool(ctx, argv, argc, 0, false) : false;
    // the original default delay was 100500; nothing ever
    // flipped g_ClientMacroPlayed there, so wait=true meant "sleep the delay"
    const uint delay = (uint)(argc >= 2 ? arg_int(ctx, argv, argc, 1, 100500) : 100500);

    auto *command = sc_make(SC_CLIENT_MACRO);
    command->ListA = macro->Actions;
    command->ListB = macro->SubActions;
    ScriptQueue_Exec(command);
    delete command;

    CScriptEngine *engine = (CScriptEngine *)JS_GetContextOpaque(ctx);
    const uint deadline = g_Ticks + delay;
    while (wait && (engine == nullptr || !engine->GetTerminated()) && g_Ticks < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        pause_gate(ctx);
    }
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// --- the file engine (fileengine.cpp; sandboxed by scriptfile.h) --------------

JSValue file_open(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic)
{
    auto *file = (XA_SCRIPT_FILE *)JS_GetOpaque2(ctx, thisVal, s_FileClass);
    if (file == nullptr)
    {
        return JS_EXCEPTION;
    }
    const string path = arg_text(ctx, argv, argc, 0, "");
    const bool checkExists = argc >= 2 ? arg_bool(ctx, argv, argc, 1, false) : false;
    const bool ok = magic == 0 ? ScriptFile_Open(file, path, checkExists)
                               : ScriptFile_Append(file, path, checkExists);
    return JS_NewBool(ctx, ok);
}

JSValue file_opened(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *)
{
    auto *file = (XA_SCRIPT_FILE *)JS_GetOpaque2(ctx, thisVal, s_FileClass);
    return file != nullptr && ScriptFile_Opened(file) ? JS_TRUE : JS_FALSE;
}

JSValue file_close(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *)
{
    auto *file = (XA_SCRIPT_FILE *)JS_GetOpaque2(ctx, thisVal, s_FileClass);
    if (file == nullptr)
    {
        return JS_EXCEPTION;
    }
    ScriptFile_Close(file);
    return JS_UNDEFINED;
}

JSValue file_readline(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *)
{
    auto *file = (XA_SCRIPT_FILE *)JS_GetOpaque2(ctx, thisVal, s_FileClass);
    if (file == nullptr)
    {
        return JS_EXCEPTION;
    }
    const string line = ScriptFile_ReadLine(file);
    return JS_NewStringLen(ctx, line.c_str(), line.size());
}

JSValue file_read(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *)
{
    auto *file = (XA_SCRIPT_FILE *)JS_GetOpaque2(ctx, thisVal, s_FileClass);
    if (file == nullptr)
    {
        return JS_EXCEPTION;
    }
    const string token = ScriptFile_Read(file);
    return JS_NewStringLen(ctx, token.c_str(), token.size());
}

JSValue file_write(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic)
{
    auto *file = (XA_SCRIPT_FILE *)JS_GetOpaque2(ctx, thisVal, s_FileClass);
    if (file == nullptr)
    {
        return JS_EXCEPTION;
    }
    const string text = arg_text(ctx, argv, argc, 0, "");
    if (magic == 0)
    {
        ScriptFile_Write(file, text);
    }
    else
    {
        ScriptFile_WriteLine(file, text);
    }
    return JS_UNDEFINED;
}

JSValue file_remove(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv)
{
    auto *file = (XA_SCRIPT_FILE *)JS_GetOpaque2(ctx, thisVal, s_FileClass);
    if (file == nullptr)
    {
        return JS_EXCEPTION;
    }
    if (argc >= 1)
    {
        ScriptFile_RemovePath(arg_text(ctx, argv, argc, 0, ""));
    }
    else
    {
        ScriptFile_Remove(file);
    }
    return JS_UNDEFINED;
}

// NewFile() - the CScriptFileObject ctor: an empty handle; Open/Append start
// the actual stdio file (script-thread side, sandboxed by scriptfile.h)
JSValue jsn_newfile(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    auto *file = new XA_SCRIPT_FILE();
    JSValue object = JS_NewObjectClass(ctx, s_FileClass);
    if (JS_IsException(object))
    {
        delete file;
        return object;
    }
    JS_SetOpaque(object, file);
    return object;
}

// --- PlayWav - DOCUMENTED DEVIATION ------------------------------------------
// The original played a local .wav through its audio backend. The port has
// no audio mixer; the call reports and does nothing.

JSValue jsn_playwav(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    Warning(Assistant, "script PlayWav is not available in this build\n");
    return JS_UNDEFINED;
}

// the CClientSelectedTile field selectors (SC_SELECTED_TILE SubTag)
enum XA_SELECTED_TILE_FIELD
{
    ST_SERIAL = 0,
    ST_GRAPHIC,
    ST_COLOR,
    ST_X,
    ST_Y,
    ST_Z,
    ST_LANDGRAPHIC,
    ST_LANDX,
    ST_LANDY,
    ST_LANDZ,
    ST_ISLANDTILE,
    ST_ISSTATICTILE,
    ST_ISGAMEOBJECT
};

// --- selected tile (clientselectedtile.cpp over g_SelectedClientWorldObject)

JSValue jsn_selectedtile(JSContext *ctx, JSValueConst, int, JSValueConst *, int magic)
{
    auto *command = sc_make(SC_SELECTED_TILE);
    command->SubTag = magic;
    ScriptQueue_Exec(command);
    switch (magic)
    {
        case ST_GRAPHIC:
        case ST_COLOR:
        case ST_LANDGRAPHIC:
            return JS_NewStringLen(
                ctx, command->ResultText.c_str(), command->ResultText.size());
        case ST_X:
        case ST_Y:
        case ST_Z:
        case ST_LANDX:
        case ST_LANDY:
        case ST_LANDZ:
            return JS_NewInt32(ctx, result_int(command));
        case ST_ISLANDTILE:
        case ST_ISSTATICTILE:
        case ST_ISGAMEOBJECT:
            return JS_NewBool(ctx, result_bool(command));
        default: // ST_SERIAL
        {
            const string text = CAssistant::SerialToText(command->ResultSerial);
            return JS_NewStringLen(ctx, text.c_str(), text.size());
        }
    }
}

// --- the text window (scriptgump.cpp; the g_TextDialog ops) -------------------
// Each op marshals SC_TEXT_WINDOW, then the pause gate (the original bodies emitted
// the command and called PauseWaiting - fire-and-forget + pause).

JSValue textwindow_op(JSContext *ctx, int op, JSValueConst *argv, int argc, int ints)
{
    auto *command = sc_make(SC_TEXT_WINDOW);
    command->SubTag = op;
    command->TextA = argc >= 1 ? arg_text(ctx, argv, argc, 0, "") : string();
    if (ints >= 1)
    {
        command->IntA = arg_int(ctx, argv, argc, 0, 0);
    }
    if (ints >= 2)
    {
        command->IntB = arg_int(ctx, argv, argc, 1, 0);
    }
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

JSValue textwindow_setpos(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    return textwindow_op(ctx, TW_SET_POS, argv, argc, 2);
}

JSValue textwindow_setsize(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    return textwindow_op(ctx, TW_SET_SIZE, argv, argc, 2);
}

JSValue textwindow_print(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    return textwindow_op(ctx, TW_PRINT, argv, argc, 0);
}

JSValue textwindow_savetofile(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    return textwindow_op(ctx, TW_SAVE, argv, argc, 0);
}

JSValue textwindow_noarg(JSContext *ctx, JSValueConst, int, JSValueConst *, int magic)
{
    return textwindow_op(ctx, magic, nullptr, 0, 0);
}

// [Q] GetGlobal(name) - the original OnCommandGetGlobal returned the stored value
// or left the empty string default.
JSValue jsn_globalget(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_GLOBAL_SET);
    command->SubTag = 2; // get
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    ScriptQueue_Exec(command);
    return JS_NewStringLen(
        ctx, command->ResultText.c_str(), command->ResultText.size());
}

// [Q] BandageSelf/BandageTarget - OnCommandBandageTarget: find the
// backpack bandage, CPacketTargetUseObject(bandage, serial).
JSValue jsn_bandage(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_CLIENT_CMD);
    command->IntA = 101; // executor switch: bandage
    command->TextA = arg_text(ctx, argv, argc, 0, "self");
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [G] the client Option* getters (OnCommandOption* reading the client config
// through g_GetInt with the EnabledCommandOptionsAccess gate).
JSValue jsn_optionget(JSContext *ctx, JSValueConst, int, JSValueConst *, int magic)
{
    auto *command = sc_make(SC_OPTIONS_GET);
    command->SubTag = magic;
    ScriptQueue_Exec(command);
    return JS_NewInt32(ctx, result_int(command));
}

// [Q] the Option*(value) setters (the same VKI ids through g_SetInt).
JSValue jsn_optionset(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_CLIENT_CMD);
    command->IntA = 300; // executor switch: option set
    command->IntB = magic;
    command->IntC = arg_int(ctx, argv, argc, 0, 0);
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// --- fix round (Phase 4b-2 wire-now): the remaining scriptengine families ---

// [G] FindFriend/FindEnemy(flags, distance) -> serial text (the CScriptEngine
// overloads -> CommandFindFriend/Enemy -> CAssistant::FindFriend/FindEnemy;
// "fast"/"finddistance" were the 1/2-arg defaults).
JSValue jsn_findfriendenemy(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_FRIEND_INFO);
    command->SubTag = 4 + magic; // 4 = friend scan, 5 = enemy scan
    command->TextA = arg_text(ctx, argv, argc, 0, "fast");
    command->TextB = arg_text(ctx, argv, argc, 1, "finddistance");
    ScriptQueue_Exec(command);
    string text = command->ResultDropped ? string() : command->ResultText;
    delete command;
    pause_gate(ctx);
    return JS_NewStringLen(ctx, text.c_str(), text.size());
}

// [G/Q] GetFriendList/GetFriendsStatus/GetEnemyList/GetEnemiesStatus (the
// checked entries; the lists are the checked entries).
JSValue jsn_friendinfo(JSContext *ctx, JSValueConst, int, JSValueConst *, int magic)
{
    auto *command = sc_make(SC_FRIEND_INFO);
    command->SubTag = magic;
    ScriptQueue_Exec(command);
    if (magic == 0 || magic == 2) // the list getters -> string arrays
    {
        JSValue array = JS_NewArray(ctx);
        int index = 0;
        if (!command->ResultDropped)
        {
            for (const string &entry : command->ResultList)
            {
                JS_SetPropertyUint32(ctx, array, (uint32_t)index++,
                    JS_NewStringLen(ctx, entry.c_str(), entry.size()));
            }
        }
        delete command;
        pause_gate(ctx);
        return array;
    }
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] AddFriend(name[, object]) / RemoveFriend(name) / ClearFriendList() and
// the enemy triple (commandmanager.cpp OnCommandAddFriend..ClearEnemyList).
JSValue jsn_friendcmd(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_FRIEND_CMD);
    command->SubTag = magic;
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    command->TextB = arg_text(ctx, argv, argc, 1, "");
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [G/Q] SaveHotkeys(name)/LoadHotkeys(name) -> bool; OnOffHotkeys() reads and
// OnOffHotkeys(state) sets the engine flag (the two overloads).
JSValue jsn_hotkeys(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_HOTKEYS);
    command->SubTag = magic;
    if (magic == 2 && argc >= 1)
    {
        command->BoolA = true; // the OnOffHotkeys(state) set form
        command->IntA = arg_bool(ctx, argv, argc, 0, true) ? 1 : 0;
    }
    else
    {
        command->TextA = arg_text(ctx, argv, argc, 0, "");
    }
    ScriptQueue_Exec(command);
    bool result = command->ResultBool;
    delete command;
    pause_gate(ctx);
    return JS_NewBool(ctx, result);
}

// [Q] UseAbility(name): the primary/secondary (+-keep) literals or a
// g_AbilityName table entry -> CPacketUseCombatAbility (OnCommandUseAbility,
// over the same packet family).
JSValue jsn_useability(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_CLIENT_CMD);
    command->IntA = 310;
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] UseWrestlingDisarm/Stun (the crossed pair is kept verbatim: Disarm -> the stun
// packet, Stun -> the disarm packet).
JSValue jsn_wrestling(JSContext *ctx, JSValueConst, int, JSValueConst *, int magic)
{
    auto *command = sc_make(SC_CLIENT_CMD);
    command->IntA = 311 + magic;
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [Q] InvokeVirtue(name): Honor/Sacrifice/Valor -> CPacketInvokeVirtueRequest
// (OnCommandInvokeVirture).
JSValue jsn_invokevirtue(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_CLIENT_CMD);
    command->IntA = 313;
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [G/Q] ClientViewRange() reads the VKI_VIEW_RANGE value (g_GetInt, no
// options-access gate - commandmanager.cpp:4036), ClientViewRange(range)
// sends CPacketClientViewRange (:4044).
JSValue jsn_viewrange(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_CLIENT_CMD);
    command->IntA = argc >= 1 ? 302 : 301;
    command->IntB = arg_int(ctx, argv, argc, 0, 0);
    ScriptQueue_Exec(command);
    int value = command->ResultInt;
    delete command;
    pause_gate(ctx);
    return argc >= 1 ? JS_UNDEFINED : JS_NewInt32(ctx, value);
}

// [Q] SetPVPCaller([serial]): with a serial the VKI_SET_PVPCALLER value is
// set; without one the targeting flow arms HandleSetPVPCaller
// (OnCommandSetPVPCaller).
JSValue jsn_setpvpcaller(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *command = sc_make(SC_CLIENT_CMD);
    command->IntA = 303;
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [G/Q] the text-window font colors (SetFontColor/GetFontColor/
// GetFontColorValue + the Characters triple; commandmanager.cpp gates them
// with EnabledCommandFontColor). 0/1/2 = set/get/value, 3/4/5 = characters.
JSValue jsn_fontcolor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_CLIENT_CMD);
    command->IntA = 330 + magic;
    command->BoolA = arg_bool(ctx, argv, argc, 0, false);
    command->TextA = arg_text(ctx, argv, argc, 1, "0x02B2");
    ScriptQueue_Exec(command);
    bool state = command->ResultBool;
    string text = command->ResultDropped ? string() : command->ResultText;
    delete command;
    pause_gate(ctx);
    if (magic == 0 || magic == 3)
    {
        return JS_UNDEFINED; // the setters
    }
    if (magic == 1 || magic == 4)
    {
        return JS_NewBool(ctx, state);
    }
    return JS_NewStringLen(ctx, text.c_str(), text.size());
}

// [Q] the paperdoll/statusbar window packets + LogOut + Screenshot (the
// OnCommandOpenPaperdoll..OnCommandCloseStatusbar, OnCommandLogOut handlers).
// 340 open / 341 close / 342 move (paperdoll), 343 show / 344 close
// (statusbar), 345 logout, 346 screenshot.
JSValue jsn_windowop(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic)
{
    auto *command = sc_make(SC_CLIENT_CMD);
    command->IntA = 340 + magic;
    command->TextA = arg_text(ctx, argv, argc, 0, "");
    command->IntB = arg_int(ctx, argv, argc, 1, 0);
    command->IntC = arg_int(ctx, argv, argc, 2, 0);
    command->BoolA = arg_bool(ctx, argv, argc, 3, true); // minimized
    ScriptQueue_Exec(command);
    delete command;
    pause_gate(ctx);
    return JS_UNDEFINED;
}

// [G] GetTargetType: the CommandHaveTarget read (cursor type + 1 while a
// target cursor is up, else 0). HaveTarget keeps
// its bool shape; this is the raw value.
JSValue jsn_targettype(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    return JS_NewInt32(ctx, result_int(sc_exec(SC_HAVE_TARGET)));
}

// [G] Contains(text, pattern[, ignoreCase]): pure JS-side helper: the pattern is a '|' separated alternative list.
JSValue jsn_contains(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    string text = arg_text(ctx, argv, argc, 0, "");
    string pattern = arg_text(ctx, argv, argc, 1, "");
    const bool ignoreCase = arg_bool(ctx, argv, argc, 2, false);
    if (text.empty() || pattern.empty())
    {
        return JS_FALSE;
    }
    if (ignoreCase)
    {
        for (char &c : text)
        {
            c = (char)tolower((unsigned char)c);
        }
        for (char &c : pattern)
        {
            c = (char)tolower((unsigned char)c);
        }
    }
    size_t start = 0;
    while (start <= pattern.size())
    {
        size_t bar = pattern.find('|', start);
        string part = pattern.substr(start, bar == string::npos ? string::npos : bar - start);
        if (!part.empty() && text.find(part) != string::npos)
        {
            return JS_TRUE;
        }
        if (bar == string::npos)
        {
            break;
        }
        start = bar + 1;
    }
    return JS_FALSE;
}

// [G] Split(text[, separator]): the default separator is a space, empty parts skipped.
JSValue jsn_split(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    string text = arg_text(ctx, argv, argc, 0, "");
    const string separator = arg_text(ctx, argv, argc, 1, " ");
    JSValue array = JS_NewArray(ctx);
    int index = 0;
    size_t start = 0;
    while (start <= text.size())
    {
        size_t hit = text.find(separator, start);
        string part = text.substr(start, hit == string::npos ? string::npos : hit - start);
        if (!part.empty()) // empty parts are skipped (both overload forms)
        {
            JS_SetPropertyUint32(
                ctx, array, (uint32_t)index++, JS_NewStringLen(ctx, part.c_str(), part.size()));
        }
        if (hit == string::npos)
        {
            break;
        }
        start = hit + separator.size();
    }
    return array;
}


// ---------------------------------------------------------------------------
// registration (the CScriptHandle::RunEngine global-object block)
// ---------------------------------------------------------------------------

namespace {

#define OBJ_FIELD_DEF(name, field) JS_CFUNC_MAGIC_DEF(name, 0, obj_get, field)
#define PLAYER_FIELD_DEF(name, field) JS_CFUNC_MAGIC_DEF(name, 0, player_get, field)

JSValue player_get(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic)
{
    // Player getters share the object-getter drain; the executor forces the
    // serial to g_PlayerSerial (CPlayerObject read g_Player directly).
    return obj_get(ctx, thisVal, argc, argv, magic);
}

const JSCFunctionListEntry GAMEOBJECT_PROTOTYPE[] = {
    OBJ_FIELD_DEF("Serial", OF_TEXT_SERIAL),
    OBJ_FIELD_DEF("Graphic", OF_TEXT_GRAPHIC),
    OBJ_FIELD_DEF("Color", OF_TEXT_COLOR),
    OBJ_FIELD_DEF("Container", OF_TEXT_CONTAINER),
    OBJ_FIELD_DEF("Name", OF_TEXT_NAME),
    OBJ_FIELD_DEF("X", OF_INT_X),
    OBJ_FIELD_DEF("Y", OF_INT_Y),
    OBJ_FIELD_DEF("Z", OF_INT_Z),
    OBJ_FIELD_DEF("Map", OF_INT_MAP),
    OBJ_FIELD_DEF("Count", OF_INT_COUNT),
    OBJ_FIELD_DEF("Flags", OF_INT_FLAGS),
    OBJ_FIELD_DEF("Mobile", OF_BOOL_MOBILE),
    OBJ_FIELD_DEF("Ignored", OF_BOOL_IGNORED),
    OBJ_FIELD_DEF("Frozen", OF_BOOL_FROZEN),
    OBJ_FIELD_DEF("Poisoned", OF_BOOL_POISONED),
    OBJ_FIELD_DEF("Flying", OF_BOOL_FLYING),
    OBJ_FIELD_DEF("YellowHits", OF_BOOL_YELLOWHITS),
    OBJ_FIELD_DEF("IgnoreCharacters", OF_BOOL_IGNORECHARS),
    OBJ_FIELD_DEF("Locked", OF_BOOL_LOCKED),
    OBJ_FIELD_DEF("WarMode", OF_BOOL_WARMODE),
    OBJ_FIELD_DEF("Hidden", OF_BOOL_HIDDEN),
    OBJ_FIELD_DEF("IsHuman", OF_BOOL_ISHUMAN),
    OBJ_FIELD_DEF("IsPlayer", OF_BOOL_ISPLAYER),
    OBJ_FIELD_DEF("IsCorpse", OF_BOOL_ISCORPSE),
    OBJ_FIELD_DEF("Layer", OF_INT_LAYER),
    OBJ_FIELD_DEF("IsMulti", OF_BOOL_ISMULTI),
    OBJ_FIELD_DEF("EquipLayer", OF_INT_EQUIPLAYER),
    JS_CFUNC_MAGIC_DEF("Hits", 0, obj_get_percent, OF_INT_HITS),
    OBJ_FIELD_DEF("MaxHits", OF_INT_MAXHITS),
    JS_CFUNC_MAGIC_DEF("Mana", 0, obj_get_percent, OF_INT_MANA),
    OBJ_FIELD_DEF("MaxMana", OF_INT_MAXMANA),
    JS_CFUNC_MAGIC_DEF("Stam", 0, obj_get_percent, OF_INT_STAM),
    OBJ_FIELD_DEF("MaxStam", OF_INT_MAXSTAM),
    OBJ_FIELD_DEF("Female", OF_BOOL_FEMALE),
    OBJ_FIELD_DEF("Race", OF_INT_RACE),
    OBJ_FIELD_DEF("Direction", OF_INT_DIRECTION),
    OBJ_FIELD_DEF("Notoriety", OF_INT_NOTORIETY),
    OBJ_FIELD_DEF("CanChangeName", OF_BOOL_CANCHANGENAME),
    OBJ_FIELD_DEF("Dead", OF_BOOL_DEAD),
    OBJ_FIELD_DEF("Exists", OF_BOOL_EXISTS),
    OBJ_FIELD_DEF("Properties", OF_TEXT_PROPERTIES),
    OBJ_FIELD_DEF("ProfileReceived", OF_BOOL_PROFILERECV),
    OBJ_FIELD_DEF("Profile", OF_TEXT_PROFILE),
    OBJ_FIELD_DEF("Title", OF_TEXT_TITLE),
    JS_CFUNC_MAGIC_DEF("Click", 0, obj_action, 0),
    JS_CFUNC_MAGIC_DEF("UseObject", 0, obj_action, 1),
    JS_CFUNC_MAGIC_DEF("RequestName", 0, obj_action, 2),
};

const JSCFunctionListEntry JOURNAL_PROTOTYPE[] = {
    JS_CFUNC_MAGIC_DEF("Text", 0, journal_get, 0),
    JS_CFUNC_MAGIC_DEF("Color", 0, journal_get, 1),
    JS_CFUNC_MAGIC_DEF("Timer", 0, journal_get, 2),
    JS_CFUNC_MAGIC_DEF("Flags", 0, journal_get, 3),
    JS_CFUNC_MAGIC_DEF("Serial", 0, journal_get, 4),
    JS_CFUNC_MAGIC_DEF("FindTextID", 0, journal_get, 5),
    JS_CFUNC_DEF("SetText", 1, journal_settext),
};

// The Client. table.
const JSCFunctionListEntry CLIENT_FUNCS[] = {
    JS_CFUNC_DEF("Wait", 1, jsn_wait),
    JS_CFUNC_MAGIC_DEF("Info", 0, jsn_simple, XC_INFO),
    JS_CFUNC_MAGIC_DEF("InfoTile", 0, jsn_simple, XC_INFOTILE),
    JS_CFUNC_MAGIC_DEF("InfoMenu", 0, jsn_simple, XC_INFOMENU),
    JS_CFUNC_MAGIC_DEF("SaveConfig", 0, jsn_simple, XC_SAVECONFIG),
    JS_CFUNC_MAGIC_DEF("Click", 0, jsn_simple, XC_CLICK),
    JS_CFUNC_MAGIC_DEF("UseObject", 0, jsn_simple, XC_USEOBJECT),
    JS_CFUNC_MAGIC_DEF("GetStatus", 0, jsn_simple, XC_GETSTATUS),
    JS_CFUNC_MAGIC_DEF("Attack", 0, jsn_simple, XC_ATTACK),
    JS_CFUNC_MAGIC_DEF("HelpGump", 0, jsn_simple, XC_HELPGUMP),
    JS_CFUNC_MAGIC_DEF("CloseUO", 0, jsn_simple, XC_CLOSEUO),
    JS_CFUNC_MAGIC_DEF("WarMode", 0, jsn_simple, XC_WARMODE),
    JS_CFUNC_MAGIC_DEF("Resend", 0, jsn_simple, XC_RESEND),
    JS_CFUNC_MAGIC_DEF("EmoteAction", 0, jsn_simple, XC_EMOTEACTION),
    JS_CFUNC_MAGIC_DEF("Hide", 0, jsn_simple, XC_HIDE),
    JS_CFUNC_MAGIC_DEF("Ignore", 0, jsn_simple, XC_IGNORE),
    JS_CFUNC_MAGIC_DEF("IgnoreReset", 0, jsn_simple, XC_IGNORERESET),
    JS_CFUNC_MAGIC_DEF("SetDressBag", 0, jsn_simple, XC_SETDRESSBAG),
    JS_CFUNC_MAGIC_DEF("UnsetDressBag", 0, jsn_simple, XC_UNSETDRESSBAG),
    JS_CFUNC_MAGIC_DEF("SetArm", 0, jsn_simple, XC_SETARM),
    JS_CFUNC_MAGIC_DEF("UnsetArm", 0, jsn_simple, XC_UNSETARM),
    JS_CFUNC_MAGIC_DEF("SetDress", 0, jsn_simple, XC_SETDRESS),
    JS_CFUNC_MAGIC_DEF("UnsetDress", 0, jsn_simple, XC_UNSETDRESS),
    JS_CFUNC_MAGIC_DEF("AddType", 0, jsn_simple, XC_ADDTYPE),
    JS_CFUNC_MAGIC_DEF("RemoveType", 0, jsn_simple, XC_REMOVETYPE),
    JS_CFUNC_MAGIC_DEF("AddObject", 0, jsn_simple, XC_ADDOBJECT),
    JS_CFUNC_MAGIC_DEF("RemoveObject", 0, jsn_simple, XC_REMOVEOBJECT),
    JS_CFUNC_MAGIC_DEF("ShowJournal", 0, jsn_simple, XC_SHOWJOURNAL),
    JS_CFUNC_MAGIC_DEF("ClearJournal", 0, jsn_simple, XC_CLEARJOURNAL),
    JS_CFUNC_MAGIC_DEF("JournalIgnoreCase", 0, jsn_simple, XC_JOURNALIGNORECASE),
    JS_CFUNC_MAGIC_DEF("BlockMoving", 0, jsn_simple, XC_BLOCKMOVING),
    JS_CFUNC_MAGIC_DEF("Track", 0, jsn_simple, XC_TRACK),
    JS_CFUNC_MAGIC_DEF("UseIgnoreList", 0, jsn_simple, XC_SETUSEDIGNORELIST),
    JS_CFUNC_MAGIC_DEF("Terminate", 0, jsn_simple, XC_TERMINATE),
    JS_CFUNC_DEF("Cast", 1, jsn_cast),
    // CastTarget(name|index, serial) is the targeted Cast overload
    JS_CFUNC_DEF("CastTarget", 2, jsn_cast),
    JS_CFUNC_DEF("UseSkill", 1, jsn_useskill),
    // UseSkillTarget(name|index, serial) is the targeted UseSkill overload
    JS_CFUNC_DEF("UseSkillTarget", 2, jsn_useskill),
    JS_CFUNC_DEF("SkillValue", 1, jsn_skillvalue),
    JS_CFUNC_DEF("Print", 1, jsn_print),
    JS_CFUNC_DEF("CharPrint", 2, jsn_charprint),
    JS_CFUNC_MAGIC_DEF("Say", 1, jsn_say, SAY_NORMAL),
    JS_CFUNC_MAGIC_DEF("SayYell", 1, jsn_say, SAY_YELL),       // "! "
    JS_CFUNC_MAGIC_DEF("SayWhisper", 1, jsn_say, SAY_WHISPER), // "; "
    JS_CFUNC_MAGIC_DEF("SayEmote", 1, jsn_say, SAY_EMOTE),     // ": "
    // Deviation: SayBroadcast prefixed "? "
    // through Say; kept plain so the packet goes out as normal speech (the
    // executor sends ST_NORMAL) - revisit with speech modes in 4b-2.
    JS_CFUNC_MAGIC_DEF("SayBroadcast", 1, jsn_say, SAY_BROADCAST),
    JS_CFUNC_MAGIC_DEF("SayGuild", 1, jsn_say, SAY_GUILD),       // "\\ "
    JS_CFUNC_MAGIC_DEF("SayAlliance", 1, jsn_say, SAY_ALLIANCE), // "| "
    JS_CFUNC_MAGIC_DEF("SayParty", 1, jsn_say, SAY_PARTY),       // "/ "
    JS_CFUNC_DEF("RenameMount", 2, jsn_renamemount),
    JS_CFUNC_MAGIC_DEF("SetLight", 0, jsn_onoff, 0),
    JS_CFUNC_MAGIC_DEF("SetWeather", 0, jsn_onoff, 1),
    JS_CFUNC_MAGIC_DEF("SetSeason", 0, jsn_onoff, 2),
    JS_CFUNC_MAGIC_DEF("SetTrack", 0, jsn_onoff, 3),
    JS_CFUNC_DEF("Sound", 1, jsn_sound),
    JS_CFUNC_DEF("Morph", 0, jsn_morph),
    JS_CFUNC_MAGIC_DEF("UseType", 0, jsn_usetype, UK_USE_TYPE),
    JS_CFUNC_MAGIC_DEF("UseFromGround", 0, jsn_usetype, UK_USE_FROM_GROUND),
    JS_CFUNC_MAGIC_DEF("UseTypeList", 0, jsn_usetype, UK_USE_TYPE_LIST),
    JS_CFUNC_MAGIC_DEF("UseFromGroundList", 0, jsn_usetype, UK_USE_FROM_GROUND_LIST),
    JS_CFUNC_MAGIC_DEF("FindType", 0, jsn_find, 0),
    JS_CFUNC_MAGIC_DEF("FindList", 0, jsn_find, 1),
    JS_CFUNC_DEF("Count", 1, jsn_count),
    JS_CFUNC_DEF("FindObject", 1, jsn_findobject),
    JS_CFUNC_DEF("ObjectExists", 1, jsn_objectexists),
    JS_CFUNC_MAGIC_DEF("GetSerial", 1, jsn_getserialtext, 400),
    JS_CFUNC_MAGIC_DEF("GetGraphic", 1, jsn_getserialtext, 401),
    JS_CFUNC_MAGIC_DEF("GetContainer", 1, jsn_getserialtext, 402),
    JS_CFUNC_MAGIC_DEF("InfoGump", 1, jsn_infogump, 3),
    JS_CFUNC_DEF("GetDistance", 1, jsn_getdistance),
    JS_CFUNC_DEF("ObjAtLayer", 1, jsn_objatlayer),
    JS_CFUNC_DEF("Now", 0, jsn_now),
    JS_CFUNC_DEF("HaveTarget", 0, jsn_havetarget),
    JS_CFUNC_MAGIC_DEF("WaitTargetObject", 0, jsn_hook, HK_WAIT_OBJECT),
    JS_CFUNC_MAGIC_DEF("TargetObject", 0, jsn_hook, HK_TARGET_OBJECT),
    JS_CFUNC_MAGIC_DEF("WaitTargetType", 0, jsn_hook, HK_WAIT_TYPE),
    JS_CFUNC_MAGIC_DEF("TargetType", 0, jsn_hook, HK_TARGET_TYPE),
    JS_CFUNC_MAGIC_DEF("WaitTargetGround", 0, jsn_hook, HK_WAIT_GROUND),
    JS_CFUNC_MAGIC_DEF("TargetGround", 0, jsn_hook, HK_TARGET_GROUND),
    JS_CFUNC_MAGIC_DEF("WaitTargetTypeList", 0, jsn_hook, HK_WAIT_TYPE_LIST),
    JS_CFUNC_MAGIC_DEF("TargetTypeList", 0, jsn_hook, HK_TARGET_TYPE_LIST),
    JS_CFUNC_MAGIC_DEF("WaitTargetGroundList", 0, jsn_hook, HK_WAIT_GROUND_LIST),
    JS_CFUNC_MAGIC_DEF("TargetGroundList", 0, jsn_hook, HK_TARGET_GROUND_LIST),
    JS_CFUNC_MAGIC_DEF("WaitTargetTile", 0, jsn_hook, HK_WAIT_TILE),
    JS_CFUNC_MAGIC_DEF("TargetTile", 0, jsn_hook, HK_TARGET_TILE),
    JS_CFUNC_MAGIC_DEF("WaitTargetTileRelative", 0, jsn_hook, HK_WAIT_TILE_REL),
    JS_CFUNC_MAGIC_DEF("TargetTileRelative", 0, jsn_hook, HK_TARGET_TILE_REL),
    JS_CFUNC_DEF("CancelWaitTarget", 0, jsn_cancelwaittarget),
    JS_CFUNC_DEF("CancelTarget", 0, jsn_canceltarget),
    JS_CFUNC_DEF("WaitMenu", 2, jsn_waitmenu),
    JS_CFUNC_DEF("CancelWaitMenu", 0, jsn_cancelwaitmenu),
    JS_CFUNC_DEF("InJournal", 1, jsn_injournal),
    JS_CFUNC_DEF("WaitJournal", 2, jsn_waitjournal),
    JS_CFUNC_DEF("JournalCount", 0, jsn_journalcount),
    JS_CFUNC_MAGIC_DEF("JournalLine", 0, jsn_journalline, 0),
    JS_CFUNC_MAGIC_DEF("LastJournalMessage", 0, jsn_journalline, 1),
    JS_CFUNC_MAGIC_DEF("CanWalk", 0, jsn_move, 0),
    JS_CFUNC_MAGIC_DEF("Step", 0, jsn_move, 1),
    JS_CFUNC_MAGIC_DEF("WalkTo", 0, jsn_move, 2),
    JS_CFUNC_MAGIC_DEF("IsWalking", 0, jsn_move, 3),
    JS_CFUNC_DEF("StopWalking", 0, jsn_stopwalking),
    JS_CFUNC_MAGIC_DEF("Arm", 0, jsn_dress, DK_ARM),
    JS_CFUNC_MAGIC_DEF("Disarm", 0, jsn_dress, DK_DISARM),
    JS_CFUNC_MAGIC_DEF("Dress", 0, jsn_dress, DK_DRESS),
    JS_CFUNC_MAGIC_DEF("Undress", 0, jsn_dress, DK_UNDRESS),
    JS_CFUNC_MAGIC_DEF("Unequip", 0, jsn_dress, DK_UNEQUIP),
    JS_CFUNC_MAGIC_DEF("Equip", 0, jsn_dress, DK_EQUIP),
    JS_CFUNC_MAGIC_DEF("EquipT", 0, jsn_dress, DK_EQUIP_T),
    JS_CFUNC_MAGIC_DEF("MoveItem", 0, jsn_moveitem, MK_MOVE_ITEM),
    JS_CFUNC_MAGIC_DEF("Drop", 0, jsn_moveitem, MK_DROP),
    JS_CFUNC_MAGIC_DEF("DropHere", 0, jsn_moveitem, MK_DROP_HERE),
    JS_CFUNC_DEF("LoadScript", 1, jsn_loadscript),
    JS_CFUNC_DEF("Exec", 1, jsn_exec),
    JS_CFUNC_DEF("ScriptRunning", 1, jsn_scriptrunning),
    JS_CFUNC_DEF("Time", 0, jsn_time),
    JS_CFUNC_DEF("Date", 0, jsn_date),
    JS_CFUNC_DEF("Random", 0, jsn_random),
    JS_CFUNC_DEF("AssistVersion", 0, jsn_assistversion),
    JS_CFUNC_DEF("Connected", 0, jsn_connected),
    JS_CFUNC_MAGIC_DEF("ClientLastTarget", 0, jsn_lastserial, 0),
    JS_CFUNC_MAGIC_DEF("ClientLastAttack", 0, jsn_lastserial, 1),
    JS_CFUNC_MAGIC_DEF("TargetSystemSerial", 0, jsn_lastserial, 2),
    JS_CFUNC_MAGIC_DEF("GetLastTargetPosition", 0, jsn_lastpos, 0),
    JS_CFUNC_MAGIC_DEF("GetLastAttackPosition", 0, jsn_lastpos, 1),
    JS_CFUNC_DEF("BuffExists", 1, jsn_buffexists),
    JS_CFUNC_DEF("GetGlobal", 1, jsn_globalget),
    JS_CFUNC_DEF("SetGlobal", 2, jsn_globalset),
    JS_CFUNC_DEF("ClearGlobals", 0, jsn_globalclear),
    // the menu engine (WaitMenu/CancelWaitMenu registered above)
    JS_CFUNC_DEF("MenuCount", 0, jsn_menucount),
    JS_CFUNC_DEF("GetMenu", 1, jsn_getmenu),
    JS_CFUNC_DEF("SelectMenu", 2, jsn_selectmenu),
    JS_CFUNC_DEF("CloseMenu", 1, jsn_closemenu),
    // the gump engine
    JS_CFUNC_DEF("CreateGumpHook", 1, jsn_creategumphook),
    JS_CFUNC_DEF("WaitGump", 1, jsn_waitgump),
    JS_CFUNC_DEF("CancelWaitGump", 0, jsn_cancelwaitgump),
    JS_CFUNC_DEF("GumpCount", 0, jsn_gumpcount),
    JS_CFUNC_DEF("GetLastGump", 0, jsn_getgump),
    JS_CFUNC_DEF("GetGump", 1, jsn_getgump),
    // prompt
    JS_CFUNC_DEF("WaitPrompt", 1, jsn_waitprompt),
    JS_CFUNC_DEF("CancelWaitPrompt", 0, jsn_cancelwaitprompt),
    JS_CFUNC_MAGIC_DEF("PromptExists", 0, jsn_promptinfo, 0),
    JS_CFUNC_MAGIC_DEF("PromptSerial", 0, jsn_promptinfo, 1),
    JS_CFUNC_MAGIC_DEF("PromptID", 0, jsn_promptinfo, 2),
    JS_CFUNC_DEF("SendPrompt", 1, jsn_sendprompt),
    // trade
    JS_CFUNC_MAGIC_DEF("TradeCount", 0, jsn_tradeinfo, 0),
    JS_CFUNC_MAGIC_DEF("TradeContainer", 2, jsn_tradeinfo, 1),
    JS_CFUNC_MAGIC_DEF("TradeOpponent", 1, jsn_tradeinfo, 2),
    JS_CFUNC_MAGIC_DEF("TradeName", 1, jsn_tradeinfo, 3),
    JS_CFUNC_MAGIC_DEF("TradeCheckState", 2, jsn_tradeinfo, 4),
    JS_CFUNC_MAGIC_DEF("TradeCheck", 1, jsn_tradecmd, 0),
    JS_CFUNC_MAGIC_DEF("TradeClose", 1, jsn_tradecmd, 1),
    // shop
    JS_CFUNC_MAGIC_DEF("Buy", 1, jsn_shop, 0),
    JS_CFUNC_MAGIC_DEF("Sell", 1, jsn_shop, 1),
    // the WaitFor* family (the 1000ms default when the delay is omitted)
    JS_CFUNC_MAGIC_DEF("WaitForTarget", 0, jsn_waitfor, WF_TARGET),
    JS_CFUNC_MAGIC_DEF("WaitForMenu", 0, jsn_waitfor, WF_MENU),
    JS_CFUNC_MAGIC_DEF("WaitForGump", 0, jsn_waitfor, WF_GUMP),
    JS_CFUNC_MAGIC_DEF("WaitForPrompt", 0, jsn_waitfor, WF_PROMPT),
    JS_CFUNC_MAGIC_DEF("WaitForShop", 0, jsn_waitfor, WF_SHOP),
    JS_CFUNC_MAGIC_DEF("WaitForTrade", 0, jsn_waitfor, WF_TRADE),
    JS_CFUNC_MAGIC_DEF("WaitForContextMenu", 0, jsn_waitfor, WF_CONTEXTMENU),
    // context menu
    JS_CFUNC_MAGIC_DEF("RequestContextMenu", 1, jsn_contextmenu, 0),
    JS_CFUNC_MAGIC_DEF("WaitContextMenu", 2, jsn_contextmenu, 1),
    JS_CFUNC_MAGIC_DEF("CancelContextMenu", 0, jsn_contextmenu, 2),
    // timers
    JS_CFUNC_MAGIC_DEF("TimerExists", 1, jsn_timer, 0),
    JS_CFUNC_MAGIC_DEF("SetTimer", 1, jsn_timer, 1),
    JS_CFUNC_MAGIC_DEF("Timer", 1, jsn_timer, 2),
    JS_CFUNC_MAGIC_DEF("RemoveTimer", 1, jsn_timer, 3),
    JS_CFUNC_MAGIC_DEF("ClearTimers", 0, jsn_timer, 4),
    JS_CFUNC_DEF("CreateClientMacro", 0, jsn_createclientmacro),
    JS_CFUNC_DEF("PlayWav", 1, jsn_playwav),
    JS_CFUNC_DEF("NewFile", 0, jsn_newfile),
    JS_CFUNC_DEF("BandageSelf", 0, jsn_bandage),
    JS_CFUNC_DEF("BandageTarget", 1, jsn_bandage),
    // client option getters (OnCommandOption*)
    JS_CFUNC_MAGIC_DEF("OptionSound", 0, jsn_optionget, VKI_SOUND),
    JS_CFUNC_MAGIC_DEF("OptionSoundVolume", 0, jsn_optionget, VKI_SOUND_VALUE),
    JS_CFUNC_MAGIC_DEF("OptionMusic", 0, jsn_optionget, VKI_MUSIC),
    JS_CFUNC_MAGIC_DEF("OptionMusicVolume", 0, jsn_optionget, VKI_MUSIC_VALUE),
    JS_CFUNC_MAGIC_DEF("OptionUseTooltips", 0, jsn_optionget, VKI_USE_TOOLTIPS),
    JS_CFUNC_MAGIC_DEF("OptionAlwaysRun", 0, jsn_optionget, VKI_ALWAYS_RUN),
    JS_CFUNC_MAGIC_DEF("OptionNewTargetSystem", 0, jsn_optionget, VKI_NEW_TARGET_SYSTEM),
    JS_CFUNC_MAGIC_DEF("OptionObjectHandles", 0, jsn_optionget, VKI_OBJECT_HANDLES),
    JS_CFUNC_MAGIC_DEF("OptionScaleSpeech", 0, jsn_optionget, VKI_SCALE_SPEECH_DELAY),
    JS_CFUNC_MAGIC_DEF("OptionScaleSpeechDelay", 0, jsn_optionget, VKI_SPEECH_DELAY),
    JS_CFUNC_MAGIC_DEF("OptionIgnoreGuildMessages", 0, jsn_optionget, VKI_IGNORE_GUILD_MESSAGES),
    JS_CFUNC_MAGIC_DEF(
        "OptionIgnoreAllianceMessages", 0, jsn_optionget, VKI_IGNORE_ALLIANCE_MESSAGES),
    JS_CFUNC_MAGIC_DEF("OptionDarkNights", 0, jsn_optionget, VKI_DARK_NIGHTS),
    JS_CFUNC_MAGIC_DEF("OptionColoredLighting", 0, jsn_optionget, VKI_COLORED_LIGHTING),
    JS_CFUNC_MAGIC_DEF("OptionCriminalActionsQuery", 0, jsn_optionget, VKI_CRIMINAL_ACTIONS_QUERY),
    JS_CFUNC_MAGIC_DEF("OptionCircleOfTransparency", 0, jsn_optionget, VKI_CIRCLETRANS),
    JS_CFUNC_MAGIC_DEF("OptionCircleOfTransparencyValue", 0, jsn_optionget, VKI_CIRCLETRANS_VALUE),
    JS_CFUNC_MAGIC_DEF("OptionLockResizingGameWindow", 0, jsn_optionget,
        VKI_LOCK_RESIZING_GAME_WINDOW),
    JS_CFUNC_MAGIC_DEF("OptionFPSValue", 0, jsn_optionget, VKI_CLIENT_FPS_VALUE),
    JS_CFUNC_MAGIC_DEF("OptionUseScalingGameWindow", 0, jsn_optionget, VKI_USE_SCALING_GAME_WINDOW),
    JS_CFUNC_MAGIC_DEF("OptionDrawStatusState", 0, jsn_optionget, VKI_DRAW_STATUS_STATE),
    JS_CFUNC_MAGIC_DEF("OptionDrawStumps", 0, jsn_optionget, VKI_DRAW_STUMPS),
    JS_CFUNC_MAGIC_DEF("OptionMarkingCaves", 0, jsn_optionget, VKI_MARKING_CAVES),
    JS_CFUNC_MAGIC_DEF("OptionNoVegetation", 0, jsn_optionget, VKI_NO_VEGETATION),
    JS_CFUNC_MAGIC_DEF("OptionNoFieldsAnimation", 0, jsn_optionget, VKI_NO_ANIMATE_FIELDS),
    JS_CFUNC_MAGIC_DEF("OptionStandardCharactesFrameRate", 0, jsn_optionget,
        VKI_STANDARD_CHARACTERS_DELAY),
    JS_CFUNC_MAGIC_DEF(
        "OptionStandardItemsFrameRate", 0, jsn_optionget, VKI_STANDARD_ITEMS_DELAY),
    JS_CFUNC_MAGIC_DEF("OptionLockGumpsMoving", 0, jsn_optionget, VKI_LOCK_GUMPS_MOVING),
    JS_CFUNC_MAGIC_DEF("OptionEnterChat", 0, jsn_optionget, VKI_CONSOLE_NEED_ENTER),
    JS_CFUNC_MAGIC_DEF("OptionHiddenCharacters", 0, jsn_optionget, VKI_HIDDEN_CHARACTERS_MODE),
    JS_CFUNC_MAGIC_DEF("OptionHiddenCharactersAlpha", 0, jsn_optionget, VKI_HIDDEN_CHARACTERS_ALPHA),
    JS_CFUNC_MAGIC_DEF("OptionHiddenCharactersModeOnlyForSelf", 0, jsn_optionget,
        VKI_HIDDEN_CHARACTERS_MODE_ONLY_FOR_SELF),
    JS_CFUNC_MAGIC_DEF("OptionTransparentSpellIcons", 0, jsn_optionget,
        VKI_TRANSPARENT_SPELL_ICONS),
    JS_CFUNC_MAGIC_DEF("OptionSpellIconsAlpha", 0, jsn_optionget, VKI_SPELL_ICONS_ALPHA),
    JS_CFUNC_MAGIC_DEF("OptionFastRotation", 0, jsn_optionget, VKI_FAST_ROTATION),
    // client option setters (the same VKI ids through the gated g_SetInt)
    JS_CFUNC_MAGIC_DEF("SetOptionSound", 0, jsn_optionset, VKI_SOUND),
    JS_CFUNC_MAGIC_DEF("SetOptionSoundVolume", 0, jsn_optionset, VKI_SOUND_VALUE),
    JS_CFUNC_MAGIC_DEF("SetOptionMusic", 0, jsn_optionset, VKI_MUSIC),
    JS_CFUNC_MAGIC_DEF("SetOptionMusicVolume", 0, jsn_optionset, VKI_MUSIC_VALUE),
    JS_CFUNC_MAGIC_DEF("SetOptionUseTooltips", 0, jsn_optionset, VKI_USE_TOOLTIPS),
    JS_CFUNC_MAGIC_DEF("SetOptionAlwaysRun", 0, jsn_optionset, VKI_ALWAYS_RUN),
    JS_CFUNC_MAGIC_DEF("SetOptionNewTargetSystem", 0, jsn_optionset, VKI_NEW_TARGET_SYSTEM),
    JS_CFUNC_MAGIC_DEF("SetOptionObjectHandles", 0, jsn_optionset, VKI_OBJECT_HANDLES),
    JS_CFUNC_MAGIC_DEF("SetOptionScaleSpeech", 0, jsn_optionset, VKI_SCALE_SPEECH_DELAY),
    JS_CFUNC_MAGIC_DEF("SetOptionScaleSpeechDelay", 0, jsn_optionset, VKI_SPEECH_DELAY),
    JS_CFUNC_MAGIC_DEF("SetOptionIgnoreGuildMessages", 0, jsn_optionset, VKI_IGNORE_GUILD_MESSAGES),
    JS_CFUNC_MAGIC_DEF(
        "SetOptionIgnoreAllianceMessages", 0, jsn_optionset, VKI_IGNORE_ALLIANCE_MESSAGES),
    JS_CFUNC_MAGIC_DEF("SetOptionDarkNights", 0, jsn_optionset, VKI_DARK_NIGHTS),
    JS_CFUNC_MAGIC_DEF("SetOptionColoredLighting", 0, jsn_optionset, VKI_COLORED_LIGHTING),
    JS_CFUNC_MAGIC_DEF(
        "SetOptionCriminalActionsQuery", 0, jsn_optionset, VKI_CRIMINAL_ACTIONS_QUERY),
    JS_CFUNC_MAGIC_DEF("SetOptionCircleOfTransparency", 0, jsn_optionset, VKI_CIRCLETRANS),
    JS_CFUNC_MAGIC_DEF(
        "SetOptionCircleOfTransparencyValue", 0, jsn_optionset, VKI_CIRCLETRANS_VALUE),
    JS_CFUNC_MAGIC_DEF(
        "SetOptionLockResizingGameWindow", 0, jsn_optionset, VKI_LOCK_RESIZING_GAME_WINDOW),
    JS_CFUNC_MAGIC_DEF("SetOptionFPSValue", 0, jsn_optionset, VKI_CLIENT_FPS_VALUE),
    JS_CFUNC_MAGIC_DEF(
        "SetOptionUseScalingGameWindow", 0, jsn_optionset, VKI_USE_SCALING_GAME_WINDOW),
    JS_CFUNC_MAGIC_DEF("SetOptionDrawStatusState", 0, jsn_optionset, VKI_DRAW_STATUS_STATE),
    JS_CFUNC_MAGIC_DEF("SetOptionDrawStumps", 0, jsn_optionset, VKI_DRAW_STUMPS),
    JS_CFUNC_MAGIC_DEF("SetOptionMarkingCaves", 0, jsn_optionset, VKI_MARKING_CAVES),
    JS_CFUNC_MAGIC_DEF("SetOptionNoVegetation", 0, jsn_optionset, VKI_NO_VEGETATION),
    JS_CFUNC_MAGIC_DEF("SetOptionNoFieldsAnimation", 0, jsn_optionset, VKI_NO_ANIMATE_FIELDS),
    JS_CFUNC_MAGIC_DEF("SetOptionStandardCharactesFrameRate", 0, jsn_optionset,
        VKI_STANDARD_CHARACTERS_DELAY),
    JS_CFUNC_MAGIC_DEF(
        "SetOptionStandardItemsFrameRate", 0, jsn_optionset, VKI_STANDARD_ITEMS_DELAY),
    JS_CFUNC_MAGIC_DEF("SetOptionLockGumpsMoving", 0, jsn_optionset, VKI_LOCK_GUMPS_MOVING),
    JS_CFUNC_MAGIC_DEF("SetOptionEnterChat", 0, jsn_optionset, VKI_CONSOLE_NEED_ENTER),
    JS_CFUNC_MAGIC_DEF("SetOptionHiddenCharacters", 0, jsn_optionset, VKI_HIDDEN_CHARACTERS_MODE),
    JS_CFUNC_MAGIC_DEF(
        "SetOptionHiddenCharactersAlpha", 0, jsn_optionset, VKI_HIDDEN_CHARACTERS_ALPHA),
    JS_CFUNC_MAGIC_DEF("SetOptionHiddenCharactersModeOnlyForSelf", 0, jsn_optionset,
        VKI_HIDDEN_CHARACTERS_MODE_ONLY_FOR_SELF),
    JS_CFUNC_MAGIC_DEF(
        "SetOptionTransparentSpellIcons", 0, jsn_optionset, VKI_TRANSPARENT_SPELL_ICONS),
    JS_CFUNC_MAGIC_DEF("SetOptionSpellIconsAlpha", 0, jsn_optionset, VKI_SPELL_ICONS_ALPHA),
    JS_CFUNC_MAGIC_DEF("SetOptionFastRotation", 0, jsn_optionset, VKI_FAST_ROTATION),
    // the friends/enemies family (CAssistant::FindFriend/FindEnemy + the
    // config lists; commandmanager.cpp OnCommandFindFriend..ClearEnemyList)
    JS_CFUNC_MAGIC_DEF("FindFriend", 0, jsn_findfriendenemy, 0),
    JS_CFUNC_MAGIC_DEF("FindEnemy", 0, jsn_findfriendenemy, 1),
    JS_CFUNC_MAGIC_DEF("GetFriendList", 0, jsn_friendinfo, 0),
    JS_CFUNC_MAGIC_DEF("GetFriendsStatus", 0, jsn_friendinfo, 1),
    JS_CFUNC_MAGIC_DEF("GetEnemyList", 0, jsn_friendinfo, 2),
    JS_CFUNC_MAGIC_DEF("GetEnemiesStatus", 0, jsn_friendinfo, 3),
    JS_CFUNC_MAGIC_DEF("AddFriend", 0, jsn_friendcmd, 0),
    JS_CFUNC_MAGIC_DEF("RemoveFriend", 0, jsn_friendcmd, 1),
    JS_CFUNC_MAGIC_DEF("ClearFriendList", 0, jsn_friendcmd, 2),
    JS_CFUNC_MAGIC_DEF("AddEnemy", 0, jsn_friendcmd, 3),
    JS_CFUNC_MAGIC_DEF("RemoveEnemy", 0, jsn_friendcmd, 4),
    JS_CFUNC_MAGIC_DEF("ClearEnemyList", 0, jsn_friendcmd, 5),
    // hotkeys (config Hotkeys.xml persistence + the engine on/off flag)
    JS_CFUNC_MAGIC_DEF("SaveHotkeys", 0, jsn_hotkeys, 0),
    JS_CFUNC_MAGIC_DEF("LoadHotkeys", 0, jsn_hotkeys, 1),
    JS_CFUNC_MAGIC_DEF("OnOffHotkeys", 0, jsn_hotkeys, 2),
    // special abilities / virtue (the CPacketUseCombatAbility family)
    JS_CFUNC_DEF("UseAbility", 1, jsn_useability),
    JS_CFUNC_MAGIC_DEF("UseWrestlingDisarm", 0, jsn_wrestling, 0),
    JS_CFUNC_MAGIC_DEF("UseWrestlingStun", 0, jsn_wrestling, 1),
    JS_CFUNC_DEF("InvokeVirtue", 1, jsn_invokevirtue),
    // client view range / pvp caller
    JS_CFUNC_DEF("ClientViewRange", 0, jsn_viewrange),
    JS_CFUNC_DEF("SetPVPCaller", 0, jsn_setpvpcaller),
    // text-window font colors (config Get/Set[Characters]FontColor)
    JS_CFUNC_MAGIC_DEF("SetFontColor", 0, jsn_fontcolor, 0),
    JS_CFUNC_MAGIC_DEF("GetFontColor", 0, jsn_fontcolor, 1),
    JS_CFUNC_MAGIC_DEF("GetFontColorValue", 0, jsn_fontcolor, 2),
    JS_CFUNC_MAGIC_DEF("SetCharactersFontColor", 0, jsn_fontcolor, 3),
    JS_CFUNC_MAGIC_DEF("GetCharactersFontColor", 0, jsn_fontcolor, 4),
    JS_CFUNC_MAGIC_DEF("GetCharactersFontColorValue", 0, jsn_fontcolor, 5),
    // paperdoll/statusbar windows + LogOut + Screenshot
    JS_CFUNC_MAGIC_DEF("OpenPaperdoll", 0, jsn_windowop, 0),
    JS_CFUNC_MAGIC_DEF("ClosePaperdoll", 0, jsn_windowop, 1),
    JS_CFUNC_MAGIC_DEF("MovePaperdoll", 0, jsn_windowop, 2),
    JS_CFUNC_MAGIC_DEF("ShowStatusbar", 0, jsn_windowop, 3),
    JS_CFUNC_MAGIC_DEF("CloseStatusbar", 0, jsn_windowop, 4),
    JS_CFUNC_MAGIC_DEF("LogOut", 0, jsn_windowop, 5),
    JS_CFUNC_MAGIC_DEF("Screenshot", 0, jsn_windowop, 6),
    // string helpers (pure script-side, the CScriptEngine helpers)
    JS_CFUNC_DEF("Contains", 2, jsn_contains),
    JS_CFUNC_DEF("Split", 1, jsn_split),
    JS_CFUNC_DEF("GetTargetType", 0, jsn_targettype),
};

// The Player. table (CPlayerObject: the getters ride the shared object
// drain; the serial is forced to the player on the main thread).
const JSCFunctionListEntry PLAYER_FUNCS[] = {
    PLAYER_FIELD_DEF("Serial", OF_TEXT_SERIAL),
    PLAYER_FIELD_DEF("Graphic", OF_TEXT_GRAPHIC),
    PLAYER_FIELD_DEF("Color", OF_TEXT_COLOR),
    PLAYER_FIELD_DEF("Container", OF_TEXT_CONTAINER),
    PLAYER_FIELD_DEF("Name", OF_TEXT_NAME),
    PLAYER_FIELD_DEF("X", OF_INT_X),
    PLAYER_FIELD_DEF("Y", OF_INT_Y),
    PLAYER_FIELD_DEF("Z", OF_INT_Z),
    PLAYER_FIELD_DEF("Map", OF_INT_MAP),
    PLAYER_FIELD_DEF("Count", OF_INT_COUNT),
    PLAYER_FIELD_DEF("Flags", OF_INT_FLAGS),
    PLAYER_FIELD_DEF("Mobile", OF_BOOL_MOBILE),
    PLAYER_FIELD_DEF("Ignored", OF_BOOL_IGNORED),
    PLAYER_FIELD_DEF("Frozen", OF_BOOL_FROZEN),
    PLAYER_FIELD_DEF("Poisoned", OF_BOOL_POISONED),
    PLAYER_FIELD_DEF("Flying", OF_BOOL_FLYING),
    PLAYER_FIELD_DEF("YellowHits", OF_BOOL_YELLOWHITS),
    PLAYER_FIELD_DEF("IgnoreCharacters", OF_BOOL_IGNORECHARS),
    PLAYER_FIELD_DEF("Locked", OF_BOOL_LOCKED),
    PLAYER_FIELD_DEF("WarMode", OF_BOOL_WARMODE),
    PLAYER_FIELD_DEF("Hidden", OF_BOOL_HIDDEN),
    PLAYER_FIELD_DEF("IsHuman", OF_BOOL_ISHUMAN),
    PLAYER_FIELD_DEF("IsPlayer", OF_BOOL_ISPLAYER),
    PLAYER_FIELD_DEF("IsCorpse", OF_BOOL_ISCORPSE),
    PLAYER_FIELD_DEF("Layer", OF_INT_LAYER),
    PLAYER_FIELD_DEF("IsMulti", OF_BOOL_ISMULTI),
    PLAYER_FIELD_DEF("EquipLayer", OF_INT_EQUIPLAYER),
    JS_CFUNC_MAGIC_DEF("Hits", 0, obj_get_percent, OF_INT_HITS),
    PLAYER_FIELD_DEF("MaxHits", OF_INT_MAXHITS),
    JS_CFUNC_MAGIC_DEF("Mana", 0, obj_get_percent, OF_INT_MANA),
    PLAYER_FIELD_DEF("MaxMana", OF_INT_MAXMANA),
    JS_CFUNC_MAGIC_DEF("Stam", 0, obj_get_percent, OF_INT_STAM),
    PLAYER_FIELD_DEF("MaxStam", OF_INT_MAXSTAM),
    PLAYER_FIELD_DEF("Female", OF_BOOL_FEMALE),
    PLAYER_FIELD_DEF("Race", OF_INT_RACE),
    PLAYER_FIELD_DEF("Direction", OF_INT_DIRECTION),
    PLAYER_FIELD_DEF("Notoriety", OF_INT_NOTORIETY),
    PLAYER_FIELD_DEF("CanChangeName", OF_BOOL_CANCHANGENAME),
    PLAYER_FIELD_DEF("Dead", OF_BOOL_DEAD),
    PLAYER_FIELD_DEF("Properties", OF_TEXT_PROPERTIES),
    PLAYER_FIELD_DEF("ProfileReceived", OF_BOOL_PROFILERECV),
    PLAYER_FIELD_DEF("Profile", OF_TEXT_PROFILE),
    PLAYER_FIELD_DEF("Title", OF_TEXT_TITLE),
    PLAYER_FIELD_DEF("Str", OF_INT_STR),
    PLAYER_FIELD_DEF("Int", OF_INT_INT),
    PLAYER_FIELD_DEF("Dex", OF_INT_DEX),
    PLAYER_FIELD_DEF("LockStrState", OF_INT_LOCKSTR),
    PLAYER_FIELD_DEF("LockIntState", OF_INT_LOCKINT),
    PLAYER_FIELD_DEF("LockDexState", OF_INT_LOCKDEX),
    JS_CFUNC_MAGIC_DEF("Weight", 0, obj_get_percent, OF_INT_WEIGHT),
    PLAYER_FIELD_DEF("MaxWeight", OF_INT_MAXWEIGHT),
    PLAYER_FIELD_DEF("Armor", OF_INT_ARMOR),
    PLAYER_FIELD_DEF("Gold", OF_INT_GOLD),
    PLAYER_FIELD_DEF("StatsCap", OF_INT_STATSCAP),
    PLAYER_FIELD_DEF("Followers", OF_INT_FOLLOWERS),
    PLAYER_FIELD_DEF("MaxFollowers", OF_INT_MAXFOLLOWERS),
    PLAYER_FIELD_DEF("FireResistance", OF_INT_FIRERES),
    PLAYER_FIELD_DEF("ColdResistance", OF_INT_COLDRES),
    PLAYER_FIELD_DEF("PoisonResistance", OF_INT_POISONRES),
    PLAYER_FIELD_DEF("EnergyResistance", OF_INT_ENERGYRES),
    PLAYER_FIELD_DEF("Luck", OF_INT_LUCK),
    PLAYER_FIELD_DEF("MinDamage", OF_INT_MINDMG),
    PLAYER_FIELD_DEF("MaxDamage", OF_INT_MAXDMG),
    PLAYER_FIELD_DEF("TithingPoints", OF_INT_TITHING),
    PLAYER_FIELD_DEF("StealthSteps", OF_INT_STEALTH),
};

// The engine-class object tables (the CScriptEngineTextWindow /
// CClientSelectedTile globals plus the Gump /
// Menu / GumpHook / File / ClientMacro prototypes).

const JSCFunctionListEntry TEXTWINDOW_FUNCS[] = {
    JS_CFUNC_MAGIC_DEF("Open", 0, textwindow_noarg, TW_OPEN),
    JS_CFUNC_MAGIC_DEF("Close", 0, textwindow_noarg, TW_CLOSE),
    JS_CFUNC_MAGIC_DEF("Clear", 0, textwindow_noarg, TW_CLEAR),
    JS_CFUNC_DEF("Print", 1, textwindow_print),
    JS_CFUNC_DEF("SetPos", 2, textwindow_setpos),
    JS_CFUNC_DEF("SetSize", 2, textwindow_setsize),
    JS_CFUNC_DEF("SaveToFile", 1, textwindow_savetofile),
};

const JSCFunctionListEntry SELECTEDTILE_FUNCS[] = {
    JS_CFUNC_MAGIC_DEF("Serial", 0, jsn_selectedtile, ST_SERIAL),
    JS_CFUNC_MAGIC_DEF("Graphic", 0, jsn_selectedtile, ST_GRAPHIC),
    JS_CFUNC_MAGIC_DEF("Color", 0, jsn_selectedtile, ST_COLOR),
    JS_CFUNC_MAGIC_DEF("X", 0, jsn_selectedtile, ST_X),
    JS_CFUNC_MAGIC_DEF("Y", 0, jsn_selectedtile, ST_Y),
    JS_CFUNC_MAGIC_DEF("Z", 0, jsn_selectedtile, ST_Z),
    JS_CFUNC_MAGIC_DEF("LandGraphic", 0, jsn_selectedtile, ST_LANDGRAPHIC),
    JS_CFUNC_MAGIC_DEF("LandX", 0, jsn_selectedtile, ST_LANDX),
    JS_CFUNC_MAGIC_DEF("LandY", 0, jsn_selectedtile, ST_LANDY),
    JS_CFUNC_MAGIC_DEF("LandZ", 0, jsn_selectedtile, ST_LANDZ),
    JS_CFUNC_MAGIC_DEF("IsLandTile", 0, jsn_selectedtile, ST_ISLANDTILE),
    JS_CFUNC_MAGIC_DEF("IsStaticTile", 0, jsn_selectedtile, ST_ISSTATICTILE),
    JS_CFUNC_MAGIC_DEF("IsGameObject", 0, jsn_selectedtile, ST_ISGAMEOBJECT),
};

const JSCFunctionListEntry GUMP_PROTOTYPE[] = {
    JS_CFUNC_MAGIC_DEF("Serial", 0, gump_get, 0),
    JS_CFUNC_MAGIC_DEF("ID", 0, gump_get, 1),
    JS_CFUNC_MAGIC_DEF("X", 0, gump_get, 2),
    JS_CFUNC_MAGIC_DEF("Y", 0, gump_get, 3),
    JS_CFUNC_MAGIC_DEF("Replayed", 0, gump_get, 4),
    JS_CFUNC_MAGIC_DEF("ReplyID", 0, gump_get, 5),
    JS_CFUNC_MAGIC_DEF("ButtonList", 0, gump_list, 0),
    JS_CFUNC_MAGIC_DEF("CheckboxList", 0, gump_list, 1),
    JS_CFUNC_MAGIC_DEF("RadioList", 0, gump_list, 2),
    JS_CFUNC_MAGIC_DEF("TilepicList", 0, gump_list, 3),
    JS_CFUNC_MAGIC_DEF("GumppicList", 0, gump_list, 4),
    JS_CFUNC_MAGIC_DEF("EntriesList", 0, gump_list, 5),
    JS_CFUNC_MAGIC_DEF("CommandList", 0, gump_list, 6),
    JS_CFUNC_MAGIC_DEF("TextList", 0, gump_list, 7),
    JS_CFUNC_DEF("Command", 1, gump_command),
    JS_CFUNC_DEF("Text", 1, gump_text),
    JS_CFUNC_DEF("Select", 1, gump_select),
    JS_CFUNC_DEF("Close", 0, gump_close),
};

const JSCFunctionListEntry MENU_PROTOTYPE[] = {
    JS_CFUNC_MAGIC_DEF("Serial", 0, menu_get, 0),
    JS_CFUNC_MAGIC_DEF("ID", 0, menu_get, 1),
    JS_CFUNC_MAGIC_DEF("Name", 0, menu_get, 2),
    JS_CFUNC_MAGIC_DEF("IsGrayMenu", 0, menu_get, 3),
    JS_CFUNC_MAGIC_DEF("ItemsCount", 0, menu_get, 4),
    JS_CFUNC_MAGIC_DEF("ItemID", 1, menu_get, 5),
    JS_CFUNC_MAGIC_DEF("ItemGraphic", 1, menu_get, 6),
    JS_CFUNC_MAGIC_DEF("ItemColor", 1, menu_get, 7),
    JS_CFUNC_MAGIC_DEF("ItemName", 1, menu_get, 8),
    JS_CFUNC_DEF("Select", 1, menu_select),
    JS_CFUNC_DEF("Close", 0, menu_close),
};

const JSCFunctionListEntry GUMPHOOK_PROTOTYPE[] = {
    JS_CFUNC_DEF("Index", 0, gumphook_get),
    JS_CFUNC_DEF("AddEntry", 2, gumphook_addentry),
    JS_CFUNC_DEF("AddCheck", 2, gumphook_addcheck),
};

const JSCFunctionListEntry FILE_PROTOTYPE[] = {
    JS_CFUNC_MAGIC_DEF("Open", 1, file_open, 0),
    JS_CFUNC_MAGIC_DEF("Append", 1, file_open, 1),
    JS_CFUNC_DEF("Opened", 0, file_opened),
    JS_CFUNC_DEF("Close", 0, file_close),
    JS_CFUNC_DEF("ReadLine", 0, file_readline),
    JS_CFUNC_DEF("Read", 0, file_read),
    JS_CFUNC_MAGIC_DEF("Write", 1, file_write, 0),
    JS_CFUNC_MAGIC_DEF("WriteLine", 1, file_write, 1),
    JS_CFUNC_DEF("Remove", 0, file_remove),
};

const JSCFunctionListEntry CLIENTMACRO_PROTOTYPE[] = {
    JS_CFUNC_DEF("AddAction", 1, macro_addaction),
    JS_CFUNC_DEF("Play", 0, macro_play),
};

// --- public entry points (apiscript.h) --------------------------------------

} // namespace <executor helpers>
} // namespace <appended natives>
} // namespace <natives>

void Script_FormatException(JSContext *ctx, string &line, string &message)
{
    // the lineNumber/toString pair: quickjs-ng carries the line
    // inside the stack frames ("<script>:N:C"), so the line is parsed from
    // there (the error property only exists for script-thrown Errors).
    JSValue exception = JS_GetException(ctx);
    line = "0";
    message = "unknown error";

    size_t size = 0;
    const char *text = JS_ToCStringLen(ctx, &size, exception);
    if (text != nullptr)
    {
        message = string(text, size);
        JS_FreeCString(ctx, text);
    }

    JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");
    const char *stackText = JS_ToCStringLen(ctx, &size, stack);
    if (stackText != nullptr)
    {
        message += "\n";
        message += stackText;
        JS_FreeCString(ctx, stackText);

        // first "<name>:<line>:" frame
        const char *frame = strstr(stackText, "<script>:");
        if (frame != nullptr)
        {
            const char *digits = frame + 9;
            if (*digits == ':')
            {
                digits++;
            }
            line.clear();
            while (isdigit((unsigned char)*digits))
            {
                line += *digits++;
            }
        }
    }
    JS_FreeValue(ctx, stack);
    JS_FreeValue(ctx, exception);
}

void Script_RegisterAPI(JSContext *ctx, const string &scriptName)
{
    JSRuntime *runtime = JS_GetRuntime(ctx);

    JS_NewClassID(runtime, &s_GameObjectClass);
    JSClassDef gameObjectClass{}; // zero-init: gc_mark/call/exotic stay null
    gameObjectClass.class_name = "GameObject";
    gameObjectClass.finalizer = gameobject_finalizer;
    JS_NewClass(runtime, s_GameObjectClass, &gameObjectClass);

    JS_NewClassID(runtime, &s_JournalClass);
    JSClassDef journalClass{}; // zero-init: gc_mark/call/exotic stay null
    journalClass.class_name = "JournalEntry";
    journalClass.finalizer = journal_finalizer;
    JS_NewClass(runtime, s_JournalClass, &journalClass);

    // The remaining engine classes (the registration bug: the ids and prototypes
    // existed but were never registered, so s_GumpClass..s_FileClass all
    // stayed 0 - aliasing s_GameObjectClass - and every new_gump_object/
    // new_menu_object/... landed in the GameObject class: the gameobject
    // finalizer then freed CGump*/CMenu* deep copies as XA_JS_GAMEOBJECT
    // (type-confused delete, real record leaked) and the instances carried
    // the wrong prototype, so obj.Select/.Open/.AddEntry were missing).
    struct XA_ENGINE_CLASS
    {
        JSClassID *Id;
        const char *Name;
        JSClassFinalizer *Finalizer;
        const JSCFunctionListEntry *Proto;
        size_t ProtoCount;
    };
    const XA_ENGINE_CLASS engineClasses[] = {
        { &s_GumpClass, "Gump", gump_finalizer, GUMP_PROTOTYPE, count_of(GUMP_PROTOTYPE) },
        { &s_MenuClass, "Menu", menu_finalizer, MENU_PROTOTYPE, count_of(MENU_PROTOTYPE) },
        { &s_GumpHookClass, "GumpHook", gumphook_finalizer, GUMPHOOK_PROTOTYPE,
            count_of(GUMPHOOK_PROTOTYPE) },
        { &s_ClientMacroClass, "ClientMacro", clientmacro_finalizer, CLIENTMACRO_PROTOTYPE,
            count_of(CLIENTMACRO_PROTOTYPE) },
        { &s_FileClass, "File", file_finalizer, FILE_PROTOTYPE, count_of(FILE_PROTOTYPE) },
    };
    for (const XA_ENGINE_CLASS &entry : engineClasses)
    {
        JS_NewClassID(runtime, entry.Id);
        JSClassDef def{};
        def.class_name = entry.Name;
        def.finalizer = entry.Finalizer;
        JS_NewClass(runtime, *entry.Id, &def);
        JSValue proto = JS_NewObject(ctx);
        JS_SetPropertyFunctionList(ctx, proto, entry.Proto, (int)entry.ProtoCount);
        JS_SetClassProto(ctx, *entry.Id, proto);
    }

    JSValue global = JS_GetGlobalObject(ctx);

    // GameObject prototype (the CScriptGameObject surface)
    JSValue gameObjectProto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(
        ctx, gameObjectProto, GAMEOBJECT_PROTOTYPE, count_of(GAMEOBJECT_PROTOTYPE));
    JS_SetClassProto(ctx, s_GameObjectClass, gameObjectProto);

    // JournalEntry prototype (the CScriptJournalObject surface)
    JSValue journalProto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(
        ctx, journalProto, JOURNAL_PROTOTYPE, count_of(JOURNAL_PROTOTYPE));
    JS_SetClassProto(ctx, s_JournalClass, journalProto);

    // Client. - the single native API table
    JSValue client = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, client, CLIENT_FUNCS, count_of(CLIENT_FUNCS));
    JS_SetPropertyStr(ctx, global, "Client", client);

    // Player. (the CPlayerObject surface)
    JSValue player = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, player, PLAYER_FUNCS, count_of(PLAYER_FUNCS));
    JS_SetPropertyStr(ctx, global, "Player", player);

    // TextWindow./SelectedTile. - the live engine objects of the original globals
    // (scripthandle.cpp:116-125); the text window state is the process
    // singleton (scriptgump.cpp), the selected tile reads are marshaled
    JSValue textWindow = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(
        ctx, textWindow, TEXTWINDOW_FUNCS, count_of(TEXTWINDOW_FUNCS));
    JS_SetPropertyStr(ctx, global, "TextWindow", textWindow);

    JSValue selectedTile = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(
        ctx, selectedTile, SELECTEDTILE_FUNCS, count_of(SELECTEDTILE_FUNCS));
    JS_SetPropertyStr(ctx, global, "SelectedTile", selectedTile);

    // The sentinel globals (scripthandle.cpp:154-165): they were STRINGS in
    // the original engine and the serial resolution pattern-matched them; they
    // stay strings so existing scripts keep working.
    static const char *const SENTINELS[] = { "self",    "ground",       "lastcorpse",
        "lasttarget", "lasttile",     "backpack", "lastcontainer", "lastattack",
        "laststatus", "lastobject" };
    for (const char *sentinel : SENTINELS)
    {
        JS_SetPropertyStr(ctx, global, sentinel, JS_NewString(ctx, sentinel));
    }

    JS_SetPropertyStr(ctx, global, "ScriptName", JS_NewStringLen(
        ctx, scriptName.c_str(), scriptName.size()));

    JS_FreeValue(ctx, global);
}

// ---------------------------------------------------------------------------
// The drain-side executor (commandqueue.h): runs on the MAIN thread inside
// ScriptQueue_Drain - the port of the original OnCommand* handler bodies + the direct
// engine reads. World access is unrestricted here (g_World/g_Player), config
// and the journal mirror are main-thread state.
// ---------------------------------------------------------------------------

namespace {

string sc_int_text(int value)
{
    char buf[32] = { 0 };
    snprintf(buf, sizeof(buf), "%i", value);
    return buf;
}

void RunDressTextCommand(const char *name, const string &arg)
{
    COMMAND_ARGS args;
    if (!arg.empty())
    {
        args.push_back(arg);
    }
    g_TextCommandManager.RunCommand(name, args);
}

CGameObject *world_object(uint serial)
{
    return g_World != nullptr ? g_World->FindWorldObject(serial) : nullptr;
}

// The SC_OBJECT_GET/SC_PLAYER_GET selector switch: one body per
// CScriptGameObject/CPlayerObject getter (the original handlers read the same
// fields through the injected interface; here it is the local world model).
bool object_field(CGameObject *object, int field, int &intValue, bool &boolValue, string &textValue)
{
    if (object == nullptr)
    {
        return false;
    }
    textValue = "";
    intValue = 0;
    boolValue = false;

    switch (field)
    {
        case OF_TEXT_SERIAL:
            textValue = CAssistant::SerialToText(object->GetSerial());
            break;
        case OF_TEXT_GRAPHIC:
            textValue = CAssistant::GraphicToText(object->GetGraphic());
            break;
        case OF_TEXT_COLOR:
            textValue = CAssistant::GraphicToText(object->GetColor());
            break;
        case OF_TEXT_CONTAINER:
            textValue = CAssistant::SerialToText(object->GetContainer());
            break;
        case OF_TEXT_NAME:
            textValue = object->GetName();
            break;
        case OF_INT_X:
            intValue = object->GetX();
            break;
        case OF_INT_Y:
            intValue = object->GetY();
            break;
        case OF_INT_Z:
            intValue = object->GetZ();
            break;
        case OF_INT_MAP:
            intValue = object->GetMapIndex();
            break;
        case OF_INT_COUNT:
            intValue = (int)object->GetCount();
            break;
        case OF_INT_FLAGS:
            intValue = object->GetFlags();
            break;
        case OF_BOOL_MOBILE:
            boolValue = object->GetNPC();
            break;
        case OF_BOOL_IGNORED:
            boolValue = object->GetIgnored();
            break;
        case OF_BOOL_FROZEN:
            boolValue = object->Frozen();
            break;
        case OF_BOOL_POISONED:
            boolValue = object->Poisoned();
            break;
        case OF_BOOL_FLYING:
            boolValue = object->Flying();
            break;
        case OF_BOOL_YELLOWHITS:
            boolValue = object->YellowHits();
            break;
        case OF_BOOL_IGNORECHARS:
            boolValue = object->IgnoreCharacters();
            break;
        case OF_BOOL_LOCKED:
            boolValue = object->Locked();
            break;
        case OF_BOOL_WARMODE:
            boolValue = object->InWarMode();
            break;
        case OF_BOOL_HIDDEN:
            boolValue = object->Hidden();
            break;
        case OF_BOOL_ISHUMAN:
            boolValue = object->IsHuman();
            break;
        case OF_BOOL_ISPLAYER:
            boolValue = object->IsPlayer();
            break;
        case OF_BOOL_ISCORPSE:
            boolValue = object->IsCorpse();
            break;
        case OF_INT_LAYER:
            intValue = ((CGameItem *)object)->GetLayer();
            break;
        case OF_BOOL_ISMULTI:
            boolValue = false; // the model has no multi flag; 4b-2 with the
                               // multi round (the original engine read the same 0)
            break;
        case OF_INT_EQUIPLAYER:
            intValue = ((CGameItem *)object)->GetEquipLayer();
            break;
        case OF_INT_HITS:
            intValue = ((CGameCharacter *)object)->GetHits();
            break;
        case OF_INT_MAXHITS:
            intValue = ((CGameCharacter *)object)->GetMaxHits();
            break;
        case OF_INT_MANA:
            intValue = ((CGameCharacter *)object)->GetMana();
            break;
        case OF_INT_MAXMANA:
            intValue = ((CGameCharacter *)object)->GetMaxMana();
            break;
        case OF_INT_STAM:
            intValue = ((CGameCharacter *)object)->GetStam();
            break;
        case OF_INT_MAXSTAM:
            intValue = ((CGameCharacter *)object)->GetMaxStam();
            break;
        case OF_BOOL_FEMALE:
            boolValue = ((CGameCharacter *)object)->GetSex();
            break;
        case OF_INT_RACE:
            intValue = ((CGameCharacter *)object)->GetRace();
            break;
        case OF_INT_DIRECTION:
            intValue = ((CGameCharacter *)object)->GetDirection();
            break;
        case OF_INT_NOTORIETY:
            intValue = ((CGameCharacter *)object)->GetNotoriety();
            break;
        case OF_BOOL_CANCHANGENAME:
            boolValue = ((CGameCharacter *)object)->GetCanChangeName();
            break;
        case OF_BOOL_DEAD:
            boolValue = object->IsCorpse() || ((CGameCharacter *)object)->GetHits() <= 0;
            break;
        case OF_BOOL_EXISTS:
            boolValue = true;
            break;
        case OF_TEXT_PROPERTIES:
            textValue = g_ObjectPropertiesManager.GetData(object->GetSerial());
            break;
        case OF_BOOL_PROFILERECV:
            boolValue = ((CGameCharacter *)object)->GetProfileReceived();
            break;
        case OF_TEXT_PROFILE:
            textValue = ((CGameCharacter *)object)->GetProfile();
            break;
        case OF_TEXT_TITLE:
            textValue = ((CGameCharacter *)object)->GetTitle();
            break;
        case OF_INT_STR:
            intValue = ((CPlayer *)object)->GetStr();
            break;
        case OF_INT_INT:
            intValue = ((CPlayer *)object)->GetInt();
            break;
        case OF_INT_DEX:
            intValue = ((CPlayer *)object)->GetDex();
            break;
        case OF_INT_LOCKSTR:
            intValue = ((CPlayer *)object)->GetLockStr();
            break;
        case OF_INT_LOCKINT:
            intValue = ((CPlayer *)object)->GetLockInt();
            break;
        case OF_INT_LOCKDEX:
            intValue = ((CPlayer *)object)->GetLockDex();
            break;
        case OF_INT_WEIGHT:
            intValue = ((CPlayer *)object)->GetWeight();
            break;
        case OF_INT_MAXWEIGHT:
            intValue = ((CPlayer *)object)->GetMaxWeight();
            break;
        case OF_INT_ARMOR:
            intValue = ((CPlayer *)object)->GetArmor();
            break;
        case OF_INT_GOLD:
            intValue = (int)((CPlayer *)object)->GetGold();
            break;
        case OF_INT_STATSCAP:
            intValue = ((CPlayer *)object)->GetStatsCap();
            break;
        case OF_INT_FOLLOWERS:
            intValue = ((CPlayer *)object)->GetFollowers();
            break;
        case OF_INT_MAXFOLLOWERS:
            intValue = ((CPlayer *)object)->GetMaxFollowers();
            break;
        case OF_INT_FIRERES:
            intValue = ((CPlayer *)object)->GetFireResistance();
            break;
        case OF_INT_COLDRES:
            intValue = ((CPlayer *)object)->GetColdResistance();
            break;
        case OF_INT_POISONRES:
            intValue = ((CPlayer *)object)->GetPoisonResistance();
            break;
        case OF_INT_ENERGYRES:
            intValue = ((CPlayer *)object)->GetEnergyResistance();
            break;
        case OF_INT_LUCK:
            intValue = ((CPlayer *)object)->GetLuck();
            break;
        case OF_INT_MINDMG:
            intValue = ((CPlayer *)object)->GetMinDamage();
            break;
        case OF_INT_MAXDMG:
            intValue = ((CPlayer *)object)->GetMaxDamage();
            break;
        case OF_INT_TITHING:
            intValue = (int)((CPlayer *)object)->GetTithingPoints();
            break;
        case OF_INT_STEALTH:
            intValue = ((CPlayer *)object)->GetStealthSteps();
            break;
        default:
            return false;
    }
    return true;
}

} // namespace

void ScriptCommand_Execute(XA_SCRIPT_COMMAND *command)
{
    if (command == nullptr)
    {
        return;
    }

    switch (command->Tag)
    {
        case SC_OBJECT_GET:
        {
            command->ResultBool = object_field(world_object(command->SerialA), command->SubTag,
                command->ResultInt, command->ResultBool, command->ResultText);
            break;
        }
        case SC_PLAYER_GET:
        {
            // the CPlayerObject getters read g_Player directly
            command->ResultBool = object_field(
                g_Player, command->SubTag, command->ResultInt, command->ResultBool,
                command->ResultText);
            break;
        }
        case SC_WAIT_PARSE:
        {
            command->ResultInt = g_AssistantConfig.DelayFromText(command->TextA);
            break;
        }
        case SC_EDITOR_LOAD:
        {
            // the editor buffer is main-thread state: the file is read here,
            // never on the script worker (jsn_loadscript marshals this)
            ScriptManager_LoadScriptFile(command->TextA);
            break;
        }
        case SC_TEXT_CMD:
        {
            command->ResultBool = g_TextCommandManager.RunCommand(command->TextA, command->ListA);
            break;
        }
        case SC_SAY:
        {
            if (g_World == nullptr)
            {
                break;
            }
            switch (command->SubTag)
            {
                case SAY_PRINT:
                case SAY_CHARPRINT:
                {
                    if (command->SubTag == SAY_PRINT)
                    {
                        // flood throttle: drop lines above ~10/s with one notice
                        uint suppressed = 0;
                        if (ScriptQueue_PrintThrottled(suppressed))
                        {
                            if (suppressed > 0)
                            {
                                g_Assistant.ClientPrint(
                                    sc_int_text((int)suppressed) + " script print lines dropped.");
                            }
                            const ushort color = command->TextA == "0xFFFF"
                                ? g_MessagesColor
                                : (ushort)strtoul(command->TextA.c_str(), nullptr, 16);
                            g_Assistant.ClientPrint(command->TextB, color);
                        }
                    }
                    else
                    {
                        g_Assistant.ClientCharPrint(CAssistant::TextToSerial(command->TextA),
                            command->TextB, command->UshortA);
                    }
                    break;
                }
                case SAY_BROADCAST:
                {
                    ushort color = 0;
                    if (!g_AssistantConfig.GetFontColor(color))
                    {
                        color = 0;
                    }
                    // the wrapper speech packet sends ST_NORMAL; broadcast
                    // becomes a plain say this round (4b-2 with speech modes)
                    CPacketUnicodeSpeechRequest(color,
                        wstring(command->TextB.begin(), command->TextB.end()))
                        .SendClient();
                    break;
                }
                default: // Say + the prefix variants
                {
                    ushort color = 0;
                    if (!g_AssistantConfig.GetFontColor(color))
                    {
                        color = 0;
                    }
                    CPacketUnicodeSpeechRequest(color,
                        wstring(command->TextB.begin(), command->TextB.end()))
                        .SendClient();
                    break;
                }
            }
            break;
        }
        case SC_CAST:
        {
            if (g_World == nullptr)
            {
                break;
            }
            if (command->BoolA) // targeted form
            {
                const uint serial = CAssistant::TextToSerial(command->TextB);
                if (!command->TextA.empty())
                {
                    g_SpellManager.TargetCast(command->TextA, serial);
                }
                else
                {
                    g_SpellManager.TargetCast(int_to_text(command->IntA), serial);
                }
            }
            else if (command->TextA.empty()) // numeric: direct packet
            {
                CPacketCastSpellRequest(command->IntA).SendClient();
            }
            else
            {
                g_SpellManager.Cast(command->TextA);
            }
            break;
        }
        case SC_USE_SKILL:
        {
            if (g_World == nullptr)
            {
                break;
            }
            if (!command->TextB.empty()) // targeted
            {
                const string name = !command->TextA.empty()
                    ? command->TextA
                    : int_to_text(command->IntA);
                g_SkillManager.TargetUse(name, CAssistant::TextToSerial(command->TextB));
            }
            else if (!command->TextA.empty())
            {
                g_SkillManager.Use(command->TextA);
            }
            else
            {
                CPacketUseSkillRequest(command->IntA).SendClient();
            }
            break;
        }
        case SC_SKILL_VALUE:
        {
            int index = command->IntA;
            if (!command->TextA.empty())
            {
                index = g_SkillManager.GetIndex(command->TextA);
            }
            const int type = CTextCommandManager::GetSkillsFlags(command->TextB);
            if (index >= 0 && index < g_SkillsCount)
            {
                const CSkill &skill = g_Skills[index];
                command->ResultInt = (int)(type == SF_BASE ? skill.GetBaseValue()
                    : type == SF_CAP ? skill.GetCap()
                                     : skill.GetValue());
                if (type == SF_LOCK)
                {
                    command->ResultInt = skill.GetStatus();
                }
            }
            break;
        }
        case SC_USE_TYPE:
        {
            if (g_World == nullptr)
            {
                break;
            }
            vector<string> list;
            CGameObject *found = nullptr;
            switch (command->SubTag)
            {
                case UK_USE_TYPE:
                    found = g_Assistant.FindType(command->ListA, command->ListB,
                        command->TextA.empty() ? "self" : command->TextA, "usedistance",
                        CTextCommandManager::GetFindFlags("fast"), "", command->BoolA, list);
                    break;
                case UK_USE_FROM_GROUND:
                    found = g_Assistant.FindType(command->ListA, command->ListB, "ground",
                        command->TextA, CTextCommandManager::GetFindFlags(command->TextB), "",
                        false, list);
                    break;
                case UK_USE_TYPE_LIST:
                    // the find-arrays gate: find arrays are a
                    // separate enabled-command bit
                    if (!EnabledCommandFindArrays())
                    {
                        break;
                    }
                    found = g_Assistant.FindList(command->ListA.empty() ? "" : command->ListA[0],
                        command->TextA, "usedistance", CTextCommandManager::GetFindFlags("fast"),
                        "", command->BoolA, list);
                    break;
                case UK_USE_FROM_GROUND_LIST:
                    // the find-arrays gate
                    if (!EnabledCommandFindArrays())
                    {
                        break;
                    }
                    found = g_Assistant.FindList(command->ListA.empty() ? "" : command->ListA[0],
                        "ground", command->TextA,
                        CTextCommandManager::GetFindFlags(command->TextB), "", false, list);
                    break;
                default:
                    break;
            }
            if (found != nullptr)
            {
                g_Assistant.DoubleClick(found->GetSerial());
                command->ResultBool = true;
            }
            break;
        }
        case SC_FIND_TYPE:
        {
            if (g_World == nullptr)
            {
                break;
            }
            if (command->SubTag == 0)
            {
                g_Assistant.FindType(command->ListA, command->ListB, command->TextA,
                    command->TextC, CTextCommandManager::GetFindFlags(command->TextB),
                    command->TextD, command->BoolA, command->ResultList);
            }
            else
            {
                g_Assistant.FindList(command->ListA.empty() ? "" : command->ListA[0],
                    command->TextA, command->TextC,
                    CTextCommandManager::GetFindFlags(command->TextB), command->TextD,
                    command->BoolA, command->ResultList);
            }
            break;
        }
        case SC_COUNT:
        {
            command->ResultInt = g_Assistant.CountType(command->ListA, command->ListB,
                command->TextA.empty() ? "self" : command->TextA,
                command->TextC.empty() ? "finddistance" : command->TextC, command->BoolA);
            break;
        }
        case SC_FIND_OBJECT:
        {
            const uint serial = CAssistant::TextToSerial(command->TextA);
            command->ResultSerial = world_object(serial) != nullptr ? serial : 0;
            break;
        }
        case SC_GET_DISTANCE:
        {
            command->ResultInt = 100500;
            if (g_World != nullptr && g_Player != nullptr)
            {
                if (!command->TextA.empty())
                {
                    CGameObject *object = world_object(CAssistant::TextToSerial(command->TextA));
                    if (object != nullptr)
                    {
                        command->ResultInt = xuoassist::GetDistance(g_Player->GetX(),
                            g_Player->GetY(), object->GetX(), object->GetY());
                    }
                }
                else
                {
                    command->ResultInt = xuoassist::GetDistance(
                        g_Player->GetX(), g_Player->GetY(), command->IntA, command->IntB);
                }
            }
            break;
        }
        case SC_OBJ_AT_LAYER:
        {
            command->ResultSerial = 0;
            if (g_World == nullptr || !EnabledCommandFindTypeOrIgnore())
            {
                break;
            }
            uint objSerial = g_PlayerSerial;
            if (!command->TextB.empty())
            {
                objSerial = CAssistant::TextToSerial(command->TextB);
            }
            CGameObject *object = world_object(objSerial);
            if (object != nullptr)
            {
                CGameItem *item = object->FindLayer(CAssistant::TextToLayer(command->TextA));
                if (item != nullptr)
                {
                    command->ResultSerial = item->GetSerial();
                }
            }
            break;
        }
        case SC_JOURNAL_FIND:
        {
            const ushort color = (ushort)strtoul(command->TextD.c_str(), nullptr, 16);
            CJournalMessage *message = ScriptJournal_Find(command->TextA,
                CAssistant::TextToSerial(command->TextC), color,
                CTextCommandManager::GetJournalFlags(command->TextB), (uint)command->IntA,
                (uint)command->IntB, command->BoolA);
            // hand the copy across the queue in the pointer slot (the old
            // uint slot truncated 64-bit heap pointers); the script thread
            // deletes it
            command->PointerA = message;
            break;
        }
        case SC_JOURNAL_INFO:
        {
            command->PointerA = nullptr;
            const vector<CJournalMessage> &messages = ScriptJournal_Messages();
            if (command->SubTag == 3) // JournalCount
            {
                command->ResultInt = (int)messages.size();
                break;
            }
            if (command->SubTag == 1) // LastJournalMessage
            {
                if (!messages.empty())
                {
                    command->PointerA = new CJournalMessage(messages.back());
                }
            }
            else // JournalLine(index[, consume])
            {
                const int index = command->IntA;
                if (index >= 0 && index < (int)messages.size())
                {
                    command->PointerA = new CJournalMessage(messages[index]);
                    if (command->BoolA)
                    {
                        // Port extension: the original JournalLine(index) had no
                        // consume and never removed. The extra argument is
                        // honored as remove-after-read.
                        ScriptJournal_RemoveAt(index);
                    }
                }
            }
            break;
        }
        case SC_HAVE_TARGET:
        {
            command->ResultInt = g_Target.IsTargeting() ? g_Target.GetCursorType() + 1 : 0;
            break;
        }
        case SC_WAIT_TARGET:
        {
            if (g_World == nullptr)
            {
                break;
            }
            switch (command->SubTag)
            {
                case HK_WAIT_OBJECT:
                case HK_TARGET_OBJECT:
                {
                    if (command->SubTag == HK_WAIT_OBJECT)
                    {
                        g_Target.SetObjectsHook(command->ListA);
                    }
                    else if (g_Target.IsTargeting())
                    {
                        g_Target.SetObjectsHook(command->ListA);
                        g_Target.CheckHook();
                        g_Target.ClearHooks();
                        CPacketCancelTarget().SendClient();
                    }
                    break;
                }
                case HK_WAIT_TILE:
                case HK_TARGET_TILE:
                case HK_WAIT_TILE_REL:
                case HK_TARGET_TILE_REL:
                {
                    const bool relative = command->SubTag >= HK_WAIT_TILE_REL;
                    const bool immediate = (command->SubTag & 1) != 0;
                    if (!immediate)
                    {
                        g_Target.SetTileHook(command->ListA.empty() ? "" : command->ListA[0],
                            command->IntA, command->IntB, command->IntC, relative);
                    }
                    else if (g_Target.IsTargeting())
                    {
                        g_Target.SetTileHook(command->ListA.empty() ? "" : command->ListA[0],
                            command->IntA, command->IntB, command->IntC, relative);
                        g_Target.CheckHook();
                        g_Target.ClearHooks();
                        CPacketCancelTarget().SendClient();
                    }
                    break;
                }
                default: // the type/ground hook families arm the same tile/object
                         // hooks after a find (the original bodies passed the found
                         // list through g_Target.SetObjectsHook / SetTileHook)
                {
                    g_Target.SetObjectsHook(command->ListA);
                    break;
                }
            }
            break;
        }
        case SC_CANCEL_WAIT_TARGET:
        {
            if (g_World != nullptr) // the original body gated on the world
            {
                g_Target.ClearHooks();
            }
            break;
        }
        case SC_TARGET_SYSTEM_SERIAL:
        {
            switch (command->SubTag)
            {
                case 0:
                    command->ResultSerial = g_LastTargetObject;
                    break;
                case 1:
                    command->ResultSerial = g_LastAttackObject;
                    break;
                default: // was VKI_NEW_TARGET_SYSTEM_SERIAL (actions.cpp)
                    command->ResultSerial = Actions_NewTargetSystemSerial();
                    break;
            }
            break;
        }
        case SC_LAST_TARGET_POS:
        {
            const CPoint2Di &position =
                command->SubTag == 0 ? g_LastTargetPosition : g_LastAttackPosition;
            command->ResultInt = position.X;
            command->IntA = position.Y;
            break;
        }
        case SC_CONNECTED:
        {
            command->ResultBool = g_Connected;
            break;
        }
        case SC_OPTIONS_GET: // OnCommandOption*:
        {                    // g_World + the EnabledCommandOptionsAccess gate
            command->ResultInt =
                g_World != nullptr && EnabledCommandOptionsAccess() && g_GetInt != nullptr
                    ? g_GetInt((VALUE_KEY_INT)command->SubTag, 0)
                    : 0;
            break;
        }
        case SC_CLIENT_CMD:
        {
            switch (command->IntA)
            {
                case 101: // bandage
                {
                    if (g_World == nullptr || g_Player == nullptr)
                    {
                        break;
                    }
                    CGameItem *bandage = g_Player->FindBandage();
                    if (bandage != nullptr)
                    {
                        CPacketTargetUseObject(bandage->GetSerial(),
                            CAssistant::TextToSerial(command->TextA))
                            .SendClient();
                    }
                    break;
                }
                case 200: // canwalk(direction, x, y, z)
                {
                    uint8_t direction = (uint8_t)command->IntB;
                    int x = command->IntC;
                    int y = (int)command->SerialA;
                    char z = (char)command->UshortA;
                    command->ResultBool =
                        EnabledCommandMoving() && g_PathFinder.CanWalk(direction, x, y, z);
                    break;
                }
                case 201: // step(direction, run)
                {
                    command->ResultBool = EnabledCommandMoving() &&
                        g_PathFinder.Walk(command->BoolA, (uint8_t)command->IntB);
                    break;
                }
                case 202: // walkto(x, y, z[, distance])
                {
                    command->ResultBool = EnabledCommandMoving() &&
                        g_PathFinder.WalkTo(command->IntB, command->IntC, (int)command->SerialA,
                            command->UshortA != 0 ? command->UshortA : 1);
                    break;
                }
                case 203: // iswalking
                {
                    command->ResultBool = g_PathFinder.AutoWalking;
                    break;
                }
                case 204: // stopwalking
                {
                    if (EnabledCommandMoving())
                    {
                        g_PathFinder.StopAutoWalk();
                    }
                    break;
                }
                case 205: // CancelTarget (OnCommandCancelTarget 4521)
                {
                    if (g_World == nullptr)
                    {
                        break;
                    }
                    if (g_Target.IsTargeting())
                    {
                        g_Target.SendCancelTarget();
                        CPacketCancelTarget().SendClient();
                    }
                    break;
                }
                case 400: // GetSerial: text normalization
                    command->ResultText = CAssistant::SerialToText(
                        CAssistant::TextToSerial(command->TextA));
                    break;
                case 401: // GetGraphic
                    command->ResultText = CAssistant::GraphicToText(
                        CAssistant::TextToGraphic(command->TextA));
                    break;
                case 402: // GetContainer: the object's container serial text
                {
                    CGameObject *object = world_object(
                        CAssistant::TextToSerial(command->TextA));
                    command->ResultText = object != nullptr
                        ? CAssistant::SerialToText(object->GetContainer())
                        : string();
                    break;
                }
                case 300: // option set (the VKI id in IntB; the
                {         // OnCommandOptionX(const bool&) g_World + options gate)
                    if (g_World != nullptr && EnabledCommandOptionsAccess() && g_SetInt != nullptr)
                    {
                        g_SetInt((VALUE_KEY_INT)command->IntB, command->IntC);
                    }
                    break;
                }
                case 301: // ClientViewRange() (no options gate)
                {
                    if (g_World == nullptr)
                    {
                        break;
                    }
                    command->ResultInt =
                        g_GetInt != nullptr ? g_GetInt(VKI_VIEW_RANGE, 0) : 0;
                    break;
                }
                case 302: // ClientViewRange(range) (SendServer)
                    if (g_World == nullptr)
                    {
                        break;
                    }
                    ::CPacketClientViewRange((uint8_t)command->IntB).Send();
                    break;
                case 303: // SetPVPCaller([serial]) (OnCommandSetPVPCaller)
                {
                    if (g_World == nullptr)
                    {
                        break;
                    }
                    if (!command->TextA.empty() && g_SetInt != nullptr)
                    {
                        g_SetInt(VKI_SET_PVPCALLER, CAssistant::TextToSerial(command->TextA));
                    }
                    else if (command->TextA.empty())
                    {
                        g_TargetHandler = &CAssistant::HandleSetPVPCaller;
                        g_Assistant.RequestTarget("Set/Unset a target as a caller");
                    }
                    break;
                }
                case 310: // UseAbility(name) -> CPacketUseCombatAbility
                {
                    if (g_World == nullptr)
                    {
                        break;
                    }
                    string name = command->TextA;
                    for (char &c : name)
                    {
                        c = (char)tolower((unsigned char)c);
                    }
                    uint8_t index = 0xFF;
                    if (name == "primary")
                    {
                        index = 0;
                    }
                    else if (name == "secondary")
                    {
                        index = 1;
                    }
                    else if (name == "primary-keep")
                    {
                        index = 2;
                    }
                    else if (name == "secondary-keep")
                    {
                        index = 3;
                    }
                    else
                    {
                        for (int i = 0; i < MAX_ABILITIES_COUNT; i++)
                        {
                            string ability = g_AbilityName[i];
                            for (char &c : ability)
                            {
                                c = (char)tolower((unsigned char)c);
                            }
                            if (ability == name)
                            {
                                index = (uint8_t)(i + 1);
                                break;
                            }
                        }
                    }
                    if (index != 0xFF)
                    {
                        CPacketUseCombatAbility(index).SendClient();
                    }
                    break;
                }
                case 311: // UseWrestlingDisarm (sends the stun packet)
                    CPacketWrestlingStun().SendServer();
                    break;
                case 312: // UseWrestlingStun (sends the disarm packet)
                    CPacketWrestlingDisarm().SendServer();
                    break;
                case 313: // InvokeVirtue(name) (OnCommandInvokeVirture)
                {
                    for (int i = 0; i < MAX_INVOKE_VIRTURE_COUNT; i++)
                    {
                        if (command->TextA == g_InvokeVirtureName[i])
                        {
                            ::CPacketInvokeVirtueRequest((uint8_t)(i + 31)).Send();
                            break;
                        }
                    }
                    break;
                }
                case 330: // SetFontColor(state[, color])
                case 333: // SetCharactersFontColor(state[, color])
                    if (g_World == nullptr || !EnabledCommandFontColor())
                    {
                        break;
                    }
                    if (command->IntA == 330)
                    {
                        g_AssistantConfig.SetFontColor(
                            command->BoolA, CAssistant::TextToGraphic(command->TextA));
                    }
                    else
                    {
                        g_AssistantConfig.SetCharactersFontColor(
                            command->BoolA, CAssistant::TextToGraphic(command->TextA));
                    }
                    break;
                case 331: // GetFontColor() -> bool
                case 334: // GetCharactersFontColor() -> bool
                {
                    if (g_World == nullptr || !EnabledCommandFontColor())
                    {
                        break;
                    }
                    ushort color = 0;
                    command->ResultBool = command->IntA == 331
                        ? g_AssistantConfig.GetFontColor(color)
                        : g_AssistantConfig.GetCharactersFontColor(color);
                    break;
                }
                case 332: // GetFontColorValue() -> text
                case 335: // GetCharactersFontColorValue() -> text
                {
                    if (g_World == nullptr || !EnabledCommandFontColor())
                    {
                        break;
                    }
                    ushort color = 0;
                    if (command->IntA == 332)
                    {
                        g_AssistantConfig.GetFontColor(color);
                    }
                    else
                    {
                        g_AssistantConfig.GetCharactersFontColor(color);
                    }
                    command->ResultText = CAssistant::GraphicToText(color);
                    break;
                }
                case 340: // OpenPaperdoll(serial) (SendServer)
                    if (g_World == nullptr || command->TextA.empty())
                    {
                        break;
                    }
                    CPacketDoubleClickRequest(CAssistant::TextToSerial(command->TextA) | 0x80000000)
                        .SendServer();
                    break;
                case 341: // ClosePaperdoll(serial)
                    if (g_World == nullptr || command->TextA.empty())
                    {
                        break;
                    }
                    CPacketClosePaperdoll(CAssistant::TextToSerial(command->TextA)).SendClient();
                    break;
                case 342: // MovePaperdoll(serial, x, y)
                    if (g_World == nullptr || command->TextA.empty())
                    {
                        break;
                    }
                    CPacketMovePaperdoll(
                        CAssistant::TextToSerial(command->TextA), command->IntB, command->IntC)
                        .SendClient();
                    break;
                case 343: // ShowStatusbar(serial, x, y[, minimized])
                    if (g_World == nullptr || !EnabledCommandStatusbars())
                    {
                        break;
                    }
                    CPacketDrawStatusbar(CAssistant::TextToSerial(command->TextA), command->IntB,
                        command->IntC, command->BoolA)
                        .SendClient();
                    break;
                case 344: // CloseStatusbar(serial)
                    if (g_World == nullptr || !EnabledCommandStatusbars())
                    {
                        break;
                    }
                    CPacketCloseStatusbar(CAssistant::TextToSerial(command->TextA)).SendClient();
                    break;
                case 345: // LogOut() (OnCommandLogOut)
                    if (EnabledCommandLogOut())
                    {
                        CPacketLogOut().SendClient();
                    }
                    break;
                case 346: // Screenshot() (the client's CScreenshotBuilder,
                          // like the original Platform::GameScreenshot)
                    g_ScreenshotBuilder.SaveScreen();
                    break;
                default:
                {
                    break;
                }
            }
            break;
        }
        case SC_TERMINATE_SCRIPT:
        {
            switch (command->SubTag)
            {
                case 0: // Terminate(name, save)
                    ScriptManager_Terminate(command->TextA, command->TextB);
                    break;
                case 1: // ScriptRunning(name)
                    command->ResultBool = ScriptManager_Running(command->TextA);
                    break;
                case 2: // Exec(name, once, args)
                    ScriptManager_Exec(command->TextA, command->BoolA, command->ListA);
                    break;
                default:
                    break;
            }
            break;
        }
        case SC_DRESS_OPS:
        {
            if (g_World == nullptr)
            {
                break;
            }
            switch (command->SubTag)
            {
                case DK_ARM:
                case DK_SET_ARM:
                    RunDressTextCommand(command->SubTag == DK_ARM ? "arm" : "setarm", command->TextA);
                    break;
                case DK_DISARM:
                case DK_UNSET_ARM:
                    if (command->SubTag == DK_DISARM)
                    {
                        g_DressAgent.Disarm();
                    }
                    else
                    {
                        RunDressTextCommand("unsetarm", command->TextA);
                    }
                    break;
                case DK_DRESS:
                case DK_SET_DRESS:
                    RunDressTextCommand(command->SubTag == DK_DRESS ? "dress" : "setdress",
                        command->TextA);
                    break;
                case DK_UNDRESS:
                case DK_UNSET_DRESS:
                    if (command->SubTag == DK_UNDRESS)
                    {
                        g_DressAgent.Undress();
                    }
                    else
                    {
                        RunDressTextCommand("unsetdress", command->TextA);
                    }
                    break;
                case DK_UNEQUIP: // Unequip(layer): MoveEquip to the backpack
                {
                    const int layer = CAssistant::TextToLayer(command->TextA);
                    CGameItem *item = g_Player != nullptr ? g_Player->FindLayer(layer) : nullptr;
                    if (item != nullptr)
                    {
                        g_Assistant.MoveEquip(item->GetSerial(), layer, g_PlayerSerial);
                    }
                    break;
                }
                case DK_EQUIP: // Equip(serial) = unequip the layer + dress step
                case DK_EQUIP_T:
                {
                    // the direct MoveEquip path of OnCommandEquip
                    const uint serial = CAssistant::TextToSerial(
                        command->SubTag == DK_EQUIP ? command->TextA : command->TextA);
                    CGameObject *object = world_object(serial);
                    if (object != nullptr && g_Player != nullptr)
                    {
                        g_Assistant.MoveEquip(serial, ((CGameItem *)object)->GetLayer(), g_PlayerSerial);
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }
        // ---------------------------------------------------------------
        // batch 2 (Phase 4b-2): the engine-class families
        // ---------------------------------------------------------------
        case SC_WAIT_MENU: // OnCommandWaitMenu/CancelWaitMenu (1702/1717)
        {
            if (g_World == nullptr)
            {
                break;
            }
            if (command->BoolA)
            {
                g_MenuManager.ClearHooks();
            }
            else
            {
                g_MenuManager.SetHook(command->TextA, command->TextB);
            }
            break;
        }
        case SC_MENU_INFO: // OnCommandMenuCount/GetMenu (3554/3564)
        {
            if (g_World == nullptr || !EnabledCommandExtendedMenu())
            {
                break;
            }
            if (command->SubTag == 0)
            {
                command->ResultInt = g_MenuManager.GetMenuCount();
                break;
            }
            string name = command->TextA;
            CMenu *menu = nullptr;
            if (name == "last" || name == "lastmenu")
            {
                menu = g_MenuManager.GetLastMenu();
            }
            else
            {
                menu = g_MenuManager.GetMenu(name);
            }
            // deep copy for the script thread (the CScriptMenuObject ctor)
            command->PointerA = menu != nullptr ? new CMenu(*menu) : nullptr;
            break;
        }
        case SC_MENU_SELECT: // OnCommandSelectMenu/CloseMenu (3581/3592)
        {
            if (g_World == nullptr || !EnabledCommandExtendedMenu())
            {
                break;
            }
            command->ResultBool =
                g_MenuManager.SendChoiceIfMenuExists(command->TextA, command->TextB, false);
            break;
        }
        case SC_GUMP_HOOK_ARM: // OnCommandWaitGump/CancelWaitGump (4204/4216)
        {
            if (g_World == nullptr)
            {
                break;
            }
            if (command->BoolA)
            {
                g_GumpManager.ClearHooks();
                break;
            }
            CGumpHook hook;
            hook.SetIndex(command->IntA);
            for (size_t i = 0; i + 1 < command->ListA.size(); i += 2)
            {
                hook.m_Entries.push_back(
                    CGumpHookEntry(atoi(command->ListA[i].c_str()), command->ListA[i + 1]));
            }
            for (size_t i = 0; i + 1 < command->ListB.size(); i += 2)
            {
                hook.m_Checks.push_back(CGumpHookCheck(
                    atoi(command->ListB[i].c_str()), command->ListB[i + 1] == "1"));
            }
            g_GumpManager.AddHook(&hook);
            break;
        }
        case SC_GUMP_INFO: // OnCommandGumpCount/GetGump (4234-4273)
        {
            if (g_World == nullptr || !EnabledCommandExtendedGump())
            {
                break;
            }
            if (command->SubTag == 0)
            {
                command->ResultInt = g_GumpManager.GetGumpCount();
                break;
            }
            if (command->SubTag == 3) // InfoGump (the dump command)
            {
                g_GumpManager.InfoGump(command->TextA);
                break;
            }
            CGump *gump = nullptr;
            if (command->SubTag == 1)
            {
                // -1 was the GetLastGump path (GetGump(-1) -> GetLastGump)
                gump = command->IntA == -1 ? g_GumpManager.GetLastGump()
                                           : g_GumpManager.GetGump((uint)command->IntA);
            }
            else
            {
                gump = g_GumpManager.GetGump(command->SerialA, (uint)command->IntB);
            }
            // deep copy for the script thread (the CScriptGumpObject ctor)
            command->PointerA = gump != nullptr ? new CGump(*gump) : nullptr;
            break;
        }
        case SC_GUMP_SELECT: // OnCommandSelectGump (4224); re-resolves the
        {                    // live gump from the copy's serial+id
            if (g_World == nullptr || !EnabledCommandExtendedGump())
            {
                break;
            }
            CGump *gump = g_GumpManager.GetGump(
                (uint)strtoul(command->TextA.c_str(), nullptr, 10),
                (uint)strtoul(command->TextB.c_str(), nullptr, 10));
            if (gump == nullptr)
            {
                break;
            }
            CGumpHook hook;
            hook.SetIndex(command->IntA);
            for (size_t i = 0; i + 1 < command->ListA.size(); i += 2)
            {
                hook.m_Entries.push_back(
                    CGumpHookEntry(atoi(command->ListA[i].c_str()), command->ListA[i + 1]));
            }
            for (size_t i = 0; i + 1 < command->ListB.size(); i += 2)
            {
                hook.m_Checks.push_back(CGumpHookCheck(
                    atoi(command->ListB[i].c_str()), command->ListB[i + 1] == "1"));
            }
            command->ResultBool = g_GumpManager.SendGumpChoice(gump, hook);
            break;
        }
        case SC_PROMPT_HOOK: // OnCommandWaitPrompt/CancelWaitPrompt (4324/4345)
        {
            if (g_World == nullptr || !EnabledCommandPrompts())
            {
                break;
            }
            if (command->BoolA)
            {
                g_PromptManager.ClearHooks();
                break;
            }
            g_PromptManager.AddHook(CPrompt(CAssistant::TextToSerial(command->TextB),
                command->TextA, (PROMPT_TYPE)command->IntA));
            break;
        }
        case SC_PROMPT_INFO: // OnCommandPromptExists/Serial/ID (4534-4556)
        {
            if (g_World == nullptr || !EnabledCommandPrompts())
            {
                break;
            }
            switch (command->SubTag)
            {
                case 0:
                    command->ResultBool = g_PromptManager.GetExists();
                    break;
                case 1:
                    command->ResultSerial = g_PromptManager.GetSerial();
                    break;
                default:
                    command->ResultSerial = g_PromptManager.GetID();
                    break;
            }
            break;
        }
        case SC_PROMPT_SEND: // OnCommandSendPrompt (4558)
        {
            if (g_World == nullptr || !EnabledCommandPrompts())
            {
                break;
            }
            g_PromptManager.Send(command->TextA);
            break;
        }
        case SC_TRADE_INFO: // OnCommandTradeCount/Container/Opponent/Name/
        {                   // CheckState (3607-3704)
            if (g_World == nullptr || !EnabledCommandSecureTrading())
            {
                break;
            }
            if (command->SubTag == 0)
            {
                command->ResultInt = g_TradeManager.Count();
                break;
            }
            CTradeWindow *trade = g_TradeManager.GetTrade(command->TextA);
            if (trade == nullptr)
            {
                break;
            }
            switch (command->SubTag)
            {
                case 1: // container: opponent != 0 -> ID1, else ID2
                    command->ResultText = CAssistant::SerialToText(
                        command->IntA != 0 ? trade->GetID1() : trade->GetID2());
                    break;
                case 2: // opponent serial
                    command->ResultText = CAssistant::SerialToText(trade->GetSerial());
                    break;
                case 3: // name
                    command->ResultText = trade->GetName();
                    break;
                default: // check state: opponent != 0 -> Check1, else Check2
                    command->ResultBool =
                        command->IntA != 0 ? trade->GetCheck1() : trade->GetCheck2();
                    break;
            }
            break;
        }
        case SC_TRADE_CMD: // OnCommandTradeCheck/Close (3706/3721)
        {
            if (g_World == nullptr || !EnabledCommandSecureTrading())
            {
                break;
            }
            CTradeWindow *trade = g_TradeManager.GetTrade(command->TextA);
            if (trade == nullptr)
            {
                break;
            }
            if (command->SubTag == 0)
            {
                CPacketSecureTradeCheck(trade->GetID1(), command->BoolA).SendClient();
            }
            else
            {
                CPacketSecureTradeClose(trade->GetID1()).SendClient();
            }
            break;
        }
        case SC_SHOP: // OnCommandShop (4367)
        {
            if (!command->BoolA && !EnabledCommandShopSell())
            {
                break;
            }
            if (command->BoolA && !EnabledCommandShopBuy())
            {
                break;
            }
            const string vendor = command->TextB;
            string text = vendor;
            if (!vendor.empty())
            {
                text += " ";
            }
            text += command->BoolA ? "buy" : "sell";
            g_ShopAgent.SendShopRequest(
                command->BoolA ? SLWS_BUY : SLWS_SELL, command->TextA, text);
            break;
        }
        case SC_SHOP_STATE: // OnCommandIsShopping (4392)
        {
            command->ResultBool = EnabledFeatureShopAgent() && g_ShopAgent.IsActiveShopping();
            break;
        }
        case SC_EVENT_WAIT: // the WaitFor* flags;
        {                   // first call clears (g_XReceived = false),
            bool *flag = nullptr; // later calls only read
            switch (command->SubTag)
            {
                case WF_TARGET:
                    flag = &g_TargetReceived;
                    break;
                case WF_MENU:
                    flag = &g_MenuReceived;
                    break;
                case WF_GUMP:
                    flag = &g_GumpReceived;
                    break;
                case WF_PROMPT:
                    flag = &g_PromptReceived;
                    break;
                case WF_SHOP:
                    flag = &g_ShopReceived;
                    break;
                case WF_TRADE:
                    flag = &g_TradeReceived;
                    break;
                default:
                    flag = &g_ContextMenuReceived;
                    break;
            }
            if (!command->BoolA)
            {
                *flag = false;
                command->BoolA = true;
            }
            command->ResultBool = *flag;
            break;
        }
        case SC_FRIEND_INFO: // the CommandFindFriend/Enemy + Get*List/
        {                    // Get*Status set
            // list reads/status target the friend gate, the enemy forms the
            // enemy gate (the same split the original slots had)
            const bool enemyForm =
                command->SubTag == 2 || command->SubTag == 3 || command->SubTag == 5;
            if (g_World == nullptr ||
                (enemyForm ? !EnabledCommandEnemyList() : !EnabledCommandFriendList()))
            {
                break;
            }
            switch (command->SubTag)
            {
                case 0: // GetFriendList: the checked entries (tab list)
                case 2: // GetEnemyList
                {
                    const vector<CWorldObjectEntry> &list =
                        command->SubTag == 2 ? g_AssistantConfig.m_Enemies
                                             : g_AssistantConfig.m_Friends;
                    for (const CWorldObjectEntry &entry : list)
                    {
                        if (entry.Enabled)
                        {
                            command->ResultList.push_back(CAssistant::SerialToText(entry.Serial));
                        }
                    }
                    break;
                }
                case 1: // GetFriendsStatus
                    g_Assistant.GetFriendsStatus();
                    break;
                case 3: // GetEnemiesStatus
                    g_Assistant.GetEnemiesStatus();
                    break;
                case 4: // FindFriend(flags, distance)
                {
                    CGameObject *found = g_Assistant.FindFriend(
                        CTextCommandManager::GetFindFlags(command->TextA), command->TextB);
                    if (found != nullptr)
                    {
                        command->ResultText = CAssistant::SerialToText(found->GetSerial());
                    }
                    break;
                }
                default: // FindEnemy(flags, distance)
                {
                    CGameObject *found = g_Assistant.FindEnemy(
                        CTextCommandManager::GetFindFlags(command->TextA), command->TextB);
                    if (found != nullptr)
                    {
                        command->ResultText = CAssistant::SerialToText(found->GetSerial());
                    }
                    break;
                }
            }
            break;
        }
        case SC_FRIEND_CMD: // Add/Remove/Clear Friend/Enemy (the original slots
        {                   // only gated g_World here)
            if (g_World == nullptr)
            {
                break;
            }
            if (command->SubTag == 2) // ClearFriendList
            {
                g_AssistantConfig.m_Friends.clear();
                break;
            }
            if (command->SubTag == 5) // ClearEnemyList
            {
                g_AssistantConfig.m_Enemies.clear();
                break;
            }
            vector<CWorldObjectEntry> &list = command->SubTag < 3 ? g_AssistantConfig.m_Friends
                                                                  : g_AssistantConfig.m_Enemies;
            const uint serial = CAssistant::TextToSerial(command->TextB);
            if (command->SubTag == 0 || command->SubTag == 3) // Add*
            {
                if (!command->TextA.empty())
                {
                    CWorldObjectEntry entry;
                    entry.Serial = serial;
                    entry.Name = command->TextA;
                    entry.Enabled = true;
                    list.push_back(entry);
                }
                break;
            }
            // Remove*(name)
            for (size_t i = 0; i < list.size(); i++)
            {
                if (list[i].Name == command->TextA)
                {
                    list.erase(list.begin() + (long)i);
                    break;
                }
            }
            break;
        }
        case SC_HOTKEYS: // SaveHotkeys/LoadHotkeys/OnOffHotkeys
        {
            switch (command->SubTag)
            {
                case 0: // SaveHotkeys(name)
                    if (g_World == nullptr)
                    {
                        break;
                    }
                    g_AssistantConfig.SaveHotkeys();
                    command->ResultBool = true;
                    break;
                case 1: // LoadHotkeys(name)
                    if (g_World == nullptr)
                    {
                        break;
                    }
                    command->ResultBool = g_AssistantConfig.LoadHotkeys();
                    break;
                default: // OnOffHotkeys() get / OnOffHotkeys(state) set
                    if (command->BoolA)
                    {
                        g_AssistantConfig.m_OnOffHotkeys = command->IntA != 0;
                    }
                    else
                    {
                        command->ResultBool = g_AssistantConfig.OnOffHotkeys();
                    }
                    break;
            }
            break;
        }
        case SC_TIMER: // the OnCommandTimer family (4462-4519)
        {
            if (!EnabledCommandTimers())
            {
                break;
            }
            switch (command->SubTag)
            {
                case 0:
                    command->ResultBool = ScriptTimer_Exists(command->TextA);
                    break;
                case 1:
                    ScriptTimer_Set(command->TextA, command->IntA);
                    break;
                case 2:
                    command->ResultInt = ScriptTimer_Get(command->TextA);
                    break;
                case 3:
                    ScriptTimer_Remove(command->TextA);
                    break;
                default:
                    ScriptTimer_Clear();
                    break;
            }
            break;
        }
        case SC_CLIENT_MACRO: // OnCommandPlayClientMacro (4442)
        {
            if (g_World == nullptr)
            {
                break;
            }
            CPacketPlayMacro(command->ListA, command->ListB).SendClient();
            break;
        }
        case SC_CONTEXT_MENU: // OnCommandRequest/Wait/CancelContextMenu (4605+)
        {
            if (g_World == nullptr || !EnabledCommandContextMenu())
            {
                break;
            }
            switch (command->SubTag)
            {
                case 0:
                    CPacketContextMenuRequest(CAssistant::TextToSerial(command->TextA))
                        .SendServer();
                    break;
                case 1:
                    g_ContextMenuManager.SetHook(
                        CAssistant::TextToSerial(command->TextA), (uint)command->IntA);
                    break;
                default:
                    g_ContextMenuManager.RemoveHook();
                    break;
            }
            break;
        }
        case SC_TEXT_WINDOW: // the OnCommandTextWindow* set (709-761, 4426+)
        {
            if (!EnabledCommandTextWindow())
            {
                break;
            }
            ScriptTextWindow_Command(
                command->SubTag, command->TextA, command->IntA, command->IntB, command->TextA);
            break;
        }
        case SC_SELECTED_TILE: // the CClientSelectedTile getters (the direct
        {                      // g_SelectedClientWorldObject reads)
            const UOI_SELECTED_TILE &tile = g_SelectedClientWorldObject;
            switch (command->SubTag)
            {
                case ST_SERIAL:
                    command->ResultSerial = tile.Serial;
                    break;
                case ST_GRAPHIC:
                    command->ResultText = CAssistant::GraphicToText(tile.Graphic);
                    break;
                case ST_COLOR:
                    command->ResultText = CAssistant::GraphicToText(tile.Color);
                    break;
                case ST_X:
                    command->ResultInt = tile.X;
                    break;
                case ST_Y:
                    command->ResultInt = tile.Y;
                    break;
                case ST_Z:
                    command->ResultInt = tile.Z;
                    break;
                case ST_LANDGRAPHIC:
                    command->ResultText = CAssistant::GraphicToText(tile.LandGraphic);
                    break;
                case ST_LANDX:
                    command->ResultInt = tile.LandX;
                    break;
                case ST_LANDY:
                    command->ResultInt = tile.LandY;
                    break;
                case ST_LANDZ:
                    command->ResultInt = tile.LandZ;
                    break;
                case ST_ISLANDTILE:
                    command->ResultBool = tile.Serial == 0xFFFFFFFF;
                    break;
                case ST_ISSTATICTILE:
                    command->ResultBool = tile.Serial == 0;
                    break;
                default:
                    command->ResultBool =
                        tile.Serial != 0u && tile.Serial != 0xFFFFFFFF;
                    break;
            }
            break;
        }
        case SC_BUFF_EXISTS: // OnCommandBuffExists (3602) over the mirror
        {
            if (g_World == nullptr || !EnabledCommandBuffs())
            {
                break;
            }
            command->ResultBool = ScriptBuffs_Exists(command->TextA);
            break;
        }
        case SC_GLOBAL_SET: // OnCommandSetGlobal/GetGlobal/ClearGlobals (3973+)
        {
            switch (command->SubTag)
            {
                case 0:
                    ScriptGlobal_Set(command->TextA, command->TextB);
                    break;
                case 1:
                    ScriptGlobal_Clear();
                    break;
                default:
                    command->ResultText = ScriptGlobal_Get(command->TextA);
                    break;
            }
            break;
        }
        default:
        {
            // SC_NONE and the retired tags: nothing to do
            break;
        }
    }
}
} // namespace xuoassist
