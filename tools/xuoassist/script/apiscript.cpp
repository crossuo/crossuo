// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The assistant JS API (batch 1, Phase 4b). Natives run on the SCRIPT thread
// and marshal through ScriptQueue_Exec; the drain-side bodies of every tag
// live in the ScriptCommand_Execute switch at the bottom and run on the MAIN
// thread (they port the Qt OnCommand* slot bodies of managers/commandmanager.cpp
// and the direct reads of script/playerengine.cpp + script/gameobjectengine.cpp).
//
// API classification (plan rule: everything queued unless obviously safe):
//   [Q] mutating calls   -> queued; main thread executes the ported handler
//   [G] state getters    -> queued; main thread reads and fills the result
//   [D] direct reads     -> only Now() (g_Ticks), Time/Date/Random/
//                           AssistVersion (pure libc), engine pause/terminate
//                           flags.
// The journal mirror (journal.cpp) is main-thread fed/read; script threads
// receive copies made inside the drain.
//
// Batch-1 surface vs scriptengine.h (508 invokables): Client./Orion. covers
// the CScriptEngine list except the engine classes delivered in 4b-2 - the
// Gump/TextWindow/Menu/Prompt/Trade/Shop/ContextMenu/ClientMacro/File ops
// print the "not yet available" notice. Player. covers the CPlayerObject
// list. Deferred methods are marked PORT-TODO(4b-2) (~60).

#include "apiscript.h"
#include "commandqueue.h"
#include "engine.h"
#include "journal.h"
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
#include "../model/commonitems/skill.h"
#include "../model/gameobjects/gamecharacter.h"
#include "../model/gameobjects/gameitem.h"
#include "../model/gameobjects/gameplayer.h"
#include "../model/gameobjects/gameworld.h"

#include "../core/packets.h" // the assistant packet wrappers
#include "Network/Packets.h" // the client packets the wrappers build on
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

// The pause gate: the Qt engine called PauseWaiting() after nearly every
// marshaled call (CScriptEngineBase::PauseWaiting, scriptenginebase.cpp:18-27).
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
// (value * 100) / max formula as CPlayerObject::Hits (playerengine.cpp:22-42).
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
    // [Q] CScriptEngine::Wait (scriptengine.cpp:29-40): the delay comes from
    // the config text table (DelayFromText) + raw ms; the sleep loop checks
    // pause/terminate every 10 ms (the QThread::msleep granularity).
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
        // JS numbers for historical Orion scripts)
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
// g_Skills[] on the main thread (scriptengine.cpp:239-265).
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

// [Q] Say + the prefix variants (the Qt engine composed the prefixes onto
// Say - scriptengine.cpp:3522-3581, exact strings: yell "! ", whisper "; ",
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
// serial comes back (Qt passed a bool* result); the find+doubleclick body is
// the OnCommandUseType family (commandmanager.cpp:1037-1128).
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
// g_Assistant.CountType on the main thread (the Qt defaults:
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
    return JS_NewInt32(ctx, result_int(sc_exec(SC_HAVE_TARGET)));
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

// [Q] CancelWaitTarget - OnCommandCancelWaitTarget (commandmanager.cpp:1692):
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

// [Q] WaitMenu(prompt, choice) - PORT-TODO(4b-2): the menu engine round; the
// Qt body armed g_MenuManager's wait hook (menumanager.cpp), which lands
// with the gump/menu classes.
JSValue jsn_waitmenu(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    g_Assistant.ClientPrint(
        "WaitMenu is not yet available (assistant menu engine pending).");
    (void)argc;
    (void)argv;
    return JS_UNDEFINED;
}

// [G] the journal query family: InJournal(...)-> entry | null,
// WaitJournal(pattern, start[, end[, flags[, serial[, color]]]]) loops with
// the pause/terminate checks (CScriptEngine::WaitJournal,
// scriptengine.cpp:894-922).
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

        // 10 ms poll, then the pause gate (the Qt loop's msleep + PauseWaiting)
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
// text on the local copy only (the Qt object mutated its own m_Message).
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

// [D] pure helpers of the Qt engine (scriptengine.cpp Time/Date/Random).
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

// [G] BuffExists(graphic) - the buff ring lands in 4b-2; returns false.
JSValue jsn_buffexists(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    (void)arg_int(ctx, argv, argc, 0, 0);
    g_Assistant.ClientPrint(
        "BuffExists is not yet available (buff tracking pending, 4b-2).");
    return JS_FALSE;
}

// PORT-TODO(4b-2): the engine-class families return this notice (the plan's
// "not yet available" journal message) - gump/textwindow/menu/prompt/trade/
// shop/context-menu/client-macro/file ops.
JSValue jsn_notavailable(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    g_Assistant.ClientPrint(
        "This script API is not yet available (Phase 4b-2 engine classes).");
    return JS_UNDEFINED;
}

// [Q] SetGlobal/GetGlobal/ClearGlobals - the Qt engine kept these in the
// (not yet ported) main-tab globals list; 4b-2 stub.
JSValue jsn_globalget(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    (void)arg_text(ctx, argv, argc, 0, "");
    g_Assistant.ClientPrint("GetGlobal is not yet available (globals list pending).");
    return JS_NewString(ctx, "");
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


// ---------------------------------------------------------------------------
// registration (the CScriptHandle::RunEngine global-object block,
// scripthandle.cpp:127-166)
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

// The Client./Orion. table (identical object bound to both names - the Qt
// engine bound one CScriptEngine instance to both globals).
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
    JS_CFUNC_DEF("UseSkill", 1, jsn_useskill),
    JS_CFUNC_DEF("SkillValue", 1, jsn_skillvalue),
    JS_CFUNC_DEF("Print", 1, jsn_print),
    JS_CFUNC_DEF("CharPrint", 2, jsn_charprint),
    JS_CFUNC_MAGIC_DEF("Say", 1, jsn_say, SAY_NORMAL),
    JS_CFUNC_MAGIC_DEF("SayYell", 1, jsn_say, SAY_YELL),       // "! "
    JS_CFUNC_MAGIC_DEF("SayWhisper", 1, jsn_say, SAY_WHISPER), // "; "
    JS_CFUNC_MAGIC_DEF("SayEmote", 1, jsn_say, SAY_EMOTE),     // ": "
    // Deviation from Qt (scriptengine.cpp:3550): SayBroadcast prefixed "? "
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
    JS_CFUNC_DEF("WaitMenu", 2, jsn_waitmenu),
    JS_CFUNC_DEF("CancelWaitMenu", 0, jsn_notavailable),
    JS_CFUNC_DEF("InJournal", 1, jsn_injournal),
    JS_CFUNC_DEF("WaitJournal", 2, jsn_waitjournal),
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
    JS_CFUNC_DEF("SetGlobal", 0, jsn_notavailable),
    JS_CFUNC_DEF("ClearGlobals", 0, jsn_notavailable),
    // PORT-TODO(4b-2): TextWindow/Gump/Menu/Prompt/Trade/Shop/ContextMenu/
    // ClientMacro/File/Debug families land with their engine classes.
    JS_CFUNC_DEF("TextWindow", 0, jsn_notavailable),
    JS_CFUNC_DEF("NewFile", 0, jsn_notavailable),
    JS_CFUNC_DEF("Debug", 0, jsn_notavailable),
    JS_CFUNC_DEF("MenuCount", 0, jsn_notavailable),
    JS_CFUNC_DEF("GetMenu", 0, jsn_notavailable),
    JS_CFUNC_DEF("SelectMenu", 0, jsn_notavailable),
    JS_CFUNC_DEF("CloseMenu", 0, jsn_notavailable),
    JS_CFUNC_DEF("WaitGump", 0, jsn_notavailable),
    JS_CFUNC_DEF("WaitPrompt", 0, jsn_notavailable),
    JS_CFUNC_DEF("Buy", 0, jsn_notavailable),
    JS_CFUNC_DEF("Sell", 0, jsn_notavailable),
    JS_CFUNC_DEF("BandageSelf", 0, jsn_bandage),
    JS_CFUNC_DEF("BandageTarget", 1, jsn_bandage),
    // client option getters (OnCommandOption* - the VKI config ids)
    JS_CFUNC_MAGIC_DEF("OptionSound", 0, jsn_optionget, VKI_SOUND),
    JS_CFUNC_MAGIC_DEF("OptionSoundVolume", 0, jsn_optionget, VKI_SOUND_VALUE),
    JS_CFUNC_MAGIC_DEF("OptionMusic", 0, jsn_optionget, VKI_MUSIC),
    JS_CFUNC_MAGIC_DEF("OptionMusicVolume", 0, jsn_optionget, VKI_MUSIC_VALUE),
    JS_CFUNC_MAGIC_DEF("OptionUseTooltips", 0, jsn_optionget, VKI_USE_TOOLTIPS),
    JS_CFUNC_MAGIC_DEF("OptionAlwaysRun", 0, jsn_optionget, VKI_ALWAYS_RUN),
    JS_CFUNC_MAGIC_DEF("OptionNewTargetSystem", 0, jsn_optionget, VKI_NEW_TARGET_SYSTEM),
    JS_CFUNC_MAGIC_DEF("OptionObjectHandles", 0, jsn_optionget, VKI_OBJECT_HANDLES),
    JS_CFUNC_MAGIC_DEF("OptionIgnoreGuildMessages", 0, jsn_optionget, VKI_IGNORE_GUILD_MESSAGES),
    JS_CFUNC_MAGIC_DEF("OptionIgnoreAllianceMessages", 0, jsn_optionget, VKI_IGNORE_ALLIANCE_MESSAGES),
    JS_CFUNC_MAGIC_DEF("OptionDarkNights", 0, jsn_optionget, VKI_DARK_NIGHTS),
    JS_CFUNC_MAGIC_DEF("OptionColoredLighting", 0, jsn_optionget, VKI_COLORED_LIGHTING),
    JS_CFUNC_MAGIC_DEF("OptionCriminalActionQuery", 0, jsn_optionget, VKI_CRIMINAL_ACTIONS_QUERY),
    JS_CFUNC_MAGIC_DEF("OptionFastRotation", 0, jsn_optionget, VKI_FAST_ROTATION),
    JS_CFUNC_MAGIC_DEF("SetOptionSound", 0, jsn_optionset, VKI_SOUND),
    JS_CFUNC_MAGIC_DEF("SetOptionMusic", 0, jsn_optionset, VKI_MUSIC),
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

// PORT-TODO(4b-2): the engine-class objects (TextWindow/SelectedTile and the
// gump/menu/prompt/trade/shop/client-macro/file families) expose the notice
// native until their engine classes land.
const JSCFunctionListEntry STUB_FUNCS[] = {
    JS_CFUNC_DEF("Add", 0, jsn_notavailable),
    JS_CFUNC_DEF("Opened", 0, jsn_notavailable),
    JS_CFUNC_DEF("Open", 0, jsn_notavailable),
    JS_CFUNC_DEF("Close", 0, jsn_notavailable),
    JS_CFUNC_DEF("Clear", 0, jsn_notavailable),
    JS_CFUNC_DEF("Print", 0, jsn_notavailable),
    JS_CFUNC_DEF("Graphic", 0, jsn_notavailable),
    JS_CFUNC_DEF("Serial", 0, jsn_notavailable),
    JS_CFUNC_DEF("X", 0, jsn_notavailable),
    JS_CFUNC_DEF("Y", 0, jsn_notavailable),
    JS_CFUNC_DEF("Z", 0, jsn_notavailable),
};

// --- public entry points (apiscript.h) --------------------------------------

} // namespace <executor helpers>
} // namespace <appended natives>
} // namespace <natives>

void Script_FormatException(JSContext *ctx, string &line, string &message)
{
    // the QScriptValue lineNumber/toString pair: quickjs-ng carries the line
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

    // Client./Orion. - one shared native table (was one QObject bound to two
    // global names, scripthandle.cpp:129-134)
    JSValue client = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, client, CLIENT_FUNCS, count_of(CLIENT_FUNCS));
    JS_SetPropertyStr(ctx, global, "Client", client);
    JS_SetPropertyStr(ctx, global, "Orion", JS_DupValue(ctx, client));

    // Player. (the CPlayerObject surface)
    JSValue player = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, player, PLAYER_FUNCS, count_of(PLAYER_FUNCS));
    JS_SetPropertyStr(ctx, global, "Player", player);

    // TextWindow./SelectedTile. - the engine classes of 4b-2 (the Qt engine
    // bound live objects here; both expose the notice natives for now)
    JSValue textWindow = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, textWindow, STUB_FUNCS, count_of(STUB_FUNCS));
    JS_SetPropertyStr(ctx, global, "TextWindow", textWindow);

    JSValue selectedTile = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, selectedTile, STUB_FUNCS, count_of(STUB_FUNCS));
    JS_SetPropertyStr(ctx, global, "SelectedTile", selectedTile);

    // The sentinel globals (scripthandle.cpp:154-165): they were STRINGS in
    // the Qt engine and the serial resolution pattern-matched them; they
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
// ScriptQueue_Drain - the port of the Qt OnCommand* slot bodies + the direct
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
// CScriptGameObject/CPlayerObject getter (the Qt handlers read the same
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
                               // multi round (the Qt engine read the same 0)
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
                    // the Qt gate (commandmanager.cpp:1105): find arrays are a
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
                    // the Qt gate (commandmanager.cpp:1120)
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
                        // Port extension: Qt's JournalLine(index) had no
                        // consume and never removed
                        // (commandmanager.cpp:4183). The extra argument is
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
                         // hooks after a find (the Qt bodies passed the found
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
            if (g_World != nullptr) // the Qt body gated on the world
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
        case SC_OPTIONS_GET:
        {
            command->ResultInt = g_GetInt != nullptr
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
                case 300: // option set (the VKI id in IntB)
                {
                    if (g_SetInt != nullptr)
                    {
                        g_SetInt((VALUE_KEY_INT)command->IntB, command->IntC);
                    }
                    break;
                }
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
        default:
        {
            // SC_NONE and the retired tags: nothing to do
            break;
        }
    }
}
} // namespace xuoassist
