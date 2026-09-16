// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The main-thread marshaling queue for the script runtime (was the
// managers/commandmanager signal hub: the engine's 434
// `emit g_CommandManager.CommandX(...)` calls crossed threads through queued
// signal connections; the result pointer arguments were the blocking part).
//
// Contract (plan, Phase 4b):
//  - ONLY script worker threads push; ONLY the main thread drains
//    (ScriptQueue_Drain, called from AssistantUI_Tick BEFORE the agent ticks
//    and before the frame draw - the same position the queued signal slots
//    ran at originally, inside the event loop).
//  - Every command carries a promise side (mutex+cv+flag): the pushing script
//    thread blocks on it with a timeout; the drain marks the result and
//    notifies. Getters and mutating calls are all queued by default.
//  - Bounded: when the queue is full the OLDEST pending command is dropped
//    (woken with Dropped=true) so a print-flood script cannot grow memory
//    forever; a journal-side throttle exists in apiscript.cpp for Client.Print.
//  - Shutdown: ScriptQueue_Shutdown drops every pending entry and refuses new
//    pushes afterwards; script threads then fall through with dropped results
//    and terminate cleanly.
//
// Ownership invariant (the exec-timeout UAF fix):
//  - XA_SCRIPT_COMMAND is the CALLER's argument/result record: the script
//    thread allocates it, fills the input fields, calls ScriptQueue_Exec,
//    reads the Result* fields, and deletes it. It never enters the queue and
//    the queue never touches it after Exec returns.
//  - Push snapshots the inputs into a queue-owned node; from the moment the
//    DRAIN pops a node it owns it through execute + result copyback + wake.
//  - The worker NEVER deletes a node. On timeout: if the node is still
//    pending the worker removes and deletes it itself (the drain cannot be
//    inside it - only the drain pops); if the drain owns it the worker only
//    marks the node Abandoned under the node mutex as its last access.
//  - Every completion path (executor, drop-oldest, shutdown sweep) copies
//    the results into the caller's record only when the wait is not
//    abandoned, then parks the node on the drain-side retire list. The
//    reclaim pass deletes retired nodes once the waiter acknowledged (Ack)
//    or abandoned - so a timed-out worker can never race a live execute.

#include "../core/global.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace xuoassist {

// What the drain side executes. One enum member per marshaled behavior; the
// classification comment for each lives next to the API registration in
// apiscript.cpp (Q = queued mutation, G = queued getter, all queued in
// batch 1 - main-thread-mutated state is never read from script threads).
enum XA_SCRIPT_COMMAND_TAG
{
    SC_NONE = 0,
    // getters (G)
    SC_PLAYER_GET,        // one of the Player.* integer getters (SubTag)
    SC_PLAYER_GET_TEXT,   // Player.Name/Properties/Profile/Title/Serial-like
    SC_OBJECT_GET,        // game object getter by serial (SubTag + Serial)
    SC_SKILL_VALUE,       // SkillValue(name|index, type)
    SC_FIND_TYPE,         // FindType -> serial text list
    SC_FIND_LIST,         // FindList -> serial text list
    SC_COUNT,             // Count
    SC_FIND_OBJECT,       // FindObject -> serial (0 = not found)
    SC_GET_DISTANCE,      // GetDistance(serial) / GetDistance(x,y)
    SC_OBJ_AT_LAYER,      // ObjAtLayer(layer[, serial]) -> serial
    SC_JOURNAL_FIND,      // InJournal/WaitJournal -> journal entry or none
    SC_JOURNAL_INFO,      // JournalCount/JournalLine/LastJournalMessage
    SC_HAVE_TARGET,       // HaveTarget
    SC_CAN_WALK,          // CanWalk(direction,x,y,z)
    SC_IS_WALKING,        // IsWalking
    SC_CONNECTED,         // Connected()
    SC_BUFF_EXISTS,       // BuffExists(graphic)
    SC_NOW,               // Now()
    SC_OPTIONS_GET,       // Option* getters (SubTag selects the option)
    SC_LAST_TARGET_POS,   // GetLastTarget/AttackPosition -> x,y,z
    SC_TARGET_SYSTEM_SERIAL, // TargetSystemSerial()/ClientLastTarget/Attack
    // mutations (Q)
    SC_CLICK,             // Click/UseObject/GetStatus/Attack by serial
    SC_USE_TYPE,          // UseType/UseFromGround (+List) -> found serial
    SC_WAIT_TARGET,       // WaitTarget* hook arming
    SC_CANCEL_WAIT_TARGET,
    SC_TARGET_SEND,       // TargetObject/Type/Ground/Tile immediate send
    SC_SAY,               // Say/Print/CharPrint + modes (SubTag = kind)
    SC_MOVE_ITEM,         // MoveItem/Drop/DropHere
    SC_WAIT_MENU,         // WaitMenu/CancelWaitMenu
    SC_CAST,              // Cast(name|index[, target])
    SC_USE_SKILL,         // UseSkill(name|index[, target])
    SC_WARMODE,           // WarMode(state)
    SC_MORPH,             // Morph(graphic)
    SC_SET_LIGHT,         // SetLight(on, value)
    SC_SET_WEATHER,       // SetWeather(on, index, count)
    SC_SET_SEASON,        // SetSeason(on, index, music)
    SC_SET_TRACK,         // SetTrack(on, x, y)
    SC_SOUND,             // Sound(id)
    SC_EMOTE_ACTION,
    SC_HIDE,
    SC_IGNORE,            // Ignore(serial, on) / IgnoreReset
    SC_JOURNAL_MOD,       // ShowJournal/ClearJournal/JournalIgnoreCase
    SC_DRESS_OPS,         // SetDressBag/UnsetDressBag/SetArm/Arm/Dress/...
    SC_EQUIP,             // Equip/EquipT/Unequip(layer)
    SC_BANDAGE,           // BandageSelf/BandageTarget
    SC_GLOBAL_SET,        // SetGlobal(name, value) / ClearGlobals
    SC_CLIENT_CMD,        // CloseUO/HelpGump/Resend/CancelTarget/Screenshot...
    SC_TERMINATE_SCRIPT,  // Terminate(name, save)
    SC_TEXT_CMD,          // run a `text command` line through the parser
    SC_WAIT_PARSE,        // Wait(text) -> delay ms (config lookup, main thread)
    SC_EDITOR_LOAD,       // LoadScript(path): read the file on the MAIN thread

    // batch 2 (Phase 4b-2): the engine-class families
    SC_MENU_INFO,         // MenuCount / GetMenu (copy via PointerA)
    SC_MENU_SELECT,       // SelectMenu / CloseMenu
    SC_GUMP_HOOK_ARM,     // WaitGump(hook) / CancelWaitGump
    SC_GUMP_INFO,         // GumpCount / GetGump (copy via PointerA)
    SC_GUMP_SELECT,       // Gump.Select/Close (re-resolved by serial+id)
    SC_PROMPT_HOOK,       // WaitPrompt / CancelWaitPrompt
    SC_PROMPT_INFO,       // PromptExists/PromptSerial/PromptID
    SC_PROMPT_SEND,       // SendPrompt
    SC_TRADE_INFO,        // TradeCount/Container/Opponent/Name/CheckState
    SC_TRADE_CMD,         // TradeCheck / TradeClose
    SC_SHOP,              // Buy/Sell request (shop agent)
    SC_SHOP_STATE,        // IsShopping (the Buy/Sell wait loop)
    SC_EVENT_WAIT,        // WaitFor* (SubTag = flag; BoolA = armed marker)
    SC_TIMER,             // Timer family (SubTag = op)
    SC_CLIENT_MACRO,      // ClientMacro Play (action lists)
    SC_CONTEXT_MENU,      // Request/Wait/CancelContextMenu
    SC_TEXT_WINDOW,       // TextWindow ops (SubTag = op)
    SC_SELECTED_TILE,     // SelectedTile getters (SubTag = field)

    // fix round (Phase 4b-2 wire-now): the remaining scriptengine families
    SC_FRIEND_INFO,       // Find Friend/Enemy + Get*List/Status (SubTag = op)
    SC_FRIEND_CMD,        // Add/Remove/Clear Friend/Enemy (SubTag = op)
    SC_HOTKEYS,           // SaveHotkeys/LoadHotkeys/OnOffHotkeys (SubTag = op)

    SC_TAG_COUNT
};

// One marshaled call. Lifetime: the pushing script thread owns it (allocate,
// fill, Exec, read the Result* fields, delete); the queue copies the inputs
// into its own node and copies the results back, so this record is valid
// before AND after Exec no matter what the queue/drain raced (see the
// ownership invariant above).
struct XA_SCRIPT_COMMAND
{
    XA_SCRIPT_COMMAND_TAG Tag{ SC_NONE };
    int SubTag{ 0 };
    int IntA{ 0 };
    int IntB{ 0 };
    int IntC{ 0 };
    uint32_t SerialA{ 0 };
    uint16_t UshortA{ 0 };
    bool BoolA{ false };
    // Pointer results ride here, never in SerialA: a 32-bit slot truncates
    // 64-bit heap pointers (the journal round used to corrupt them).
    void *PointerA{ nullptr };
    astr_t TextA;
    astr_t TextB;
    astr_t TextC;
    astr_t TextD;
    vector<astr_t> ListA;
    vector<astr_t> ListB;

    // result side (written by the drain, read by the caller after Exec)
    int ResultInt{ 0 };
    uint32_t ResultSerial{ 0 };
    bool ResultBool{ false };
    bool ResultDropped{ false }; // true = no data (timeout/drop/shutdown)
    astr_t ResultText;
    vector<astr_t> ResultList;
};
static_assert(sizeof(void *) <= sizeof(uintptr_t),
    "pointer results must fit an address-sized slot");

// The drain-side executor for every tag - implemented in apiscript.cpp,
// runs on the main thread (were the original OnCommand* slot bodies).
void ScriptCommand_Execute(XA_SCRIPT_COMMAND *command);

// Pushes a command and blocks the calling (script) thread until the main
// thread completes it, `timeoutMs` elapses, or the entry is dropped/shut
// down. Returns with Result* filled and ResultDropped set when the value
// must be treated as "no data" (timeout/shutdown). The command record stays
// caller-owned: the caller reads the results and deletes it afterwards.
void ScriptQueue_Exec(XA_SCRIPT_COMMAND *command, uint32_t timeoutMs = 2000);

// The main-thread drain: executes every pending command in order. Called
// once per frame from AssistantUI_Tick (before the agent ticks). Draining is
// bounded per frame (flood control): at most ScriptQueue_MaxDrain() entries
// are executed per call, the rest wait for the next frame.
void ScriptQueue_Drain();

// Shutdown path: drops all pending entries (waking their waiters) and makes
// ScriptQueue_Exec fail fast for late pushers. Never blocks.
void ScriptQueue_Shutdown();

// Test hook: re-arms the queue after Shutdown (the client never calls this -
// a fresh process starts fresh; the harness restarts the flow in-process).
void ScriptQueue_Reset();

// Queue capacity (drop-oldest beyond this). The default covers interactive
// scripts many times over; the drop policy is the flood protection.
uint32_t ScriptQueue_Capacity();
void ScriptQueue_SetCapacity(uint32_t entries);

// Journal-side flood throttle state (used by Client.Print): returns true when
// a line with this stamp may be emitted (>= 100 ms since the last one).
bool ScriptQueue_PrintThrottled(uint32_t &suppressedCount);

} // namespace xuoassist
