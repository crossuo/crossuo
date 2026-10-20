// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The script-runtime harness (XUO_ASSISTANT_SCRIPT_TESTS=ON build target
// xuoassist-script-test). Exercises the pieces that do not need the client:
//   1. QuickJS-ng eval + function call + exception line reporting (the
//      engine.cpp flow, without the client-facing API table).
//   2. The command queue: ordered exec, bounded drop-oldest, shutdown drain,
//      push-after-shutdown fail-fast.
//   3. The journal mirror: Find pattern/flags/time filters + remove.
//   4. Pointer marshaling: the result pointer slot survives a round trip
//      through the queue even for a heap block above the 4GB boundary (the
//      old uint32_t slot truncated it).
//   5. The REAL API registration (apiscript.cpp Script_RegisterAPI) over a
//      live QuickJS context: the engine classes (Gump/File/GumpHook/
//      ClientMacro) must carry their own prototypes - the registration bug had
//      every class alias the GameObject class, so .Select/.Open/.AddEntry
//      were missing and the finalizers freed the wrong opaque types. The
//      client-facing symbols are stubbed in harnessstubs.cpp (disconnected
//      client); no live queue drain here - the scripts run through the real
//      executors directly.

#include "../commandqueue.h"
#include "../journal.h"
#include "../scriptfile.h"
#include "../../core/textcommandmanager.h" // the Client.* text-command table
#include "../scriptlist.h"
#include "../scriptgump.h"
#include "../scriptstate.h"
#include "../scripthandle.h" // the run/terminate lifecycle suite

#include "../../config/configio.h" // AtomicFileWrite + CIniFile (the config/save primitives)
#include "../../config/assistantconfig.h" // the Options/Hotkeys INI round trips
#include "../../managers/mapsettings.h" // the Map.ini round trip
#include "../../core/global.h"
#include "../../core/assistantevents.h" // JMT_* (the journal type flags)
#include "../../core/world.h" // g_World stand-in (section 6 executor gate)
#include "../apiscript.h"
#include "../../managers/gumpmanager.h"
#include "../../core/textconv.h" // ParseTextTokens (the tokenizer parity suite)

extern "C" {
#include "../quickjs/quickjs.h"
}

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring> // memcmp
#include <stdlib.h> // mkdtemp
#include <sys/stat.h> // chmod
#include <unistd.h> // rmdir, chdir
#include <cstdlib> // mkdtemp
#include <thread>
#include <vector>

// the client normally provides g_Ticks (Globals.cpp)
using ::astr_t;
uint32_t g_Ticks = 0;

// the stack-line parser copy of apiscript.cpp Script_FormatException (the
// full function needs the client world; the parser is what the harness checks)
static void harness_format_line(JSContext *ctx, astr_t &line, astr_t &message)
{
    JSValue exception = JS_GetException(ctx);
    line = "0";
    message = "unknown error";
    size_t size = 0;
    const char *text = JS_ToCStringLen(ctx, &size, exception);
    if (text != nullptr)
    {
        message = astr_t(text, size);
        JS_FreeCString(ctx, text);
    }
    JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");
    const char *stackText = JS_ToCStringLen(ctx, &size, stack);
    if (stackText != nullptr)
    {
        message += "\n";
        message += stackText;
        JS_FreeCString(ctx, stackText);
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

namespace xuoassist {

// the fake gump the executor deep-copies for GetGump (see harnessstubs.cpp)
extern GumpData *g_HarnessGump;

} // namespace xuoassist

using namespace xuoassist;

template <typename T, size_t N> constexpr size_t count_of(const T (&)[N])
{
    return N;
}

static int failures = 0;
#define CHECK(cond)                                                  \
    do                                                               \
    {                                                                \
        if (!(cond))                                                 \
        {                                                            \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            failures++;                                              \
        }                                                            \
    } while (0)

int main()
{
    // --- 1. QuickJS smoke script ----------------------------------------
    {
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx = JS_NewContext(rt);
        CHECK(ctx != nullptr);

        // the script shape: functions on the global object + a Wait-like
        const char *script = "function hello(name) { return 'hi ' + name; }\n"
                             "var boom = function () { nope(); };\n";
        JSValue result = JS_Eval(ctx, script, strlen(script), "<script>", JS_EVAL_TYPE_GLOBAL);
        CHECK(!JS_IsException(result));
        JS_FreeValue(ctx, result);

        JSValue global = JS_GetGlobalObject(ctx);
        JSValue func = JS_GetPropertyStr(ctx, global, "hello");
        CHECK(JS_IsFunction(ctx, func));
        JSValue arg = JS_NewString(ctx, "self");
        JSValue callResult = JS_Call(ctx, func, JS_UNDEFINED, 1, &arg);
        size_t size = 0;
        const char *text = JS_ToCStringLen(ctx, &size, callResult);
        CHECK(text != nullptr && strcmp(text, "hi self") == 0);
        printf("smoke: hello('self') -> %s\n", text ? text : "?");
        JS_FreeCString(ctx, text);
        JS_FreeValue(ctx, callResult);
        JS_FreeValue(ctx, arg);
        JS_FreeValue(ctx, func);

        // the error path reports the line through the stack frames (the
        // quickjs-ng replacement of the original lineNumber accessor; verified via
        // Script_FormatException's parser)
        JSValue errorFn = JS_GetPropertyStr(ctx, global, "boom");
        JSValue errorResult = JS_Call(ctx, errorFn, JS_UNDEFINED, 0, nullptr);
        CHECK(JS_IsException(errorResult));
        astr_t line;
        astr_t message;
        harness_format_line(ctx, line, message);
        CHECK(line == "2");
        printf("smoke: exception line = %s (%s)\n", line.c_str(),
            message.substr(0, message.find('\n')).c_str());
        JS_FreeValue(ctx, errorResult);
        JS_FreeValue(ctx, errorFn);
        JS_FreeValue(ctx, global);

        // the interrupt handler aborts a tight loop (the Terminate path)
        std::atomic<int> stop{ 0 };
        JS_SetInterruptHandler(rt, [](JSRuntime *, void *opaque) -> int {
            return ((std::atomic<int> *)opaque)->load() == 0;
        }, &stop);
        JSValue loop = JS_Eval(ctx, "function spin(){ while(true){} }",
            strlen("function spin(){ while(true){} }"), "<script>", JS_EVAL_TYPE_GLOBAL);
        CHECK(!JS_IsException(loop));
        JS_FreeValue(ctx, loop);
        std::thread killer([&stop]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            stop = 1;
        });
        JSValue spinFn = JS_GetPropertyStr(ctx, global, "spin");
        JSValue spinResult = JS_Call(ctx, spinFn, JS_UNDEFINED, 0, nullptr);
        CHECK(JS_IsException(spinResult)); // interrupted, not hung
        killer.join();
        JS_FreeValue(ctx, spinResult);
        JS_FreeValue(ctx, spinFn);
        printf("smoke: interrupt handler stopped while(true) in 100ms\n");

        JS_FreeValue(ctx, global);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }

    // --- 2. command queue (the REAL apiscript.cpp drain executor) ------------
    {
        // the "main thread": a background drainer like AssistantUI_Tick
        std::atomic<int> draining{ 1 };
        std::thread drainer([](std::atomic<int> *running) {
            while (running->load())
            {
                ScriptQueue_Drain();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }, &draining);
        ScriptQueue_SetCapacity(8);
        // ordered exec: the real SC_WAIT_PARSE body reads the config delay
        // helper (the real assistantconfig.cpp is linked: atoi of the text);
        // SC_NOW falls through the real
        // executor's switch (handled tags only) and copies back the defaults
        auto *first = new XA_SCRIPT_COMMAND();
        first->Tag = SC_WAIT_PARSE;
        first->TextA = "100";
        auto *second = new XA_SCRIPT_COMMAND();
        second->Tag = SC_NOW;
        ScriptQueue_Exec(first);
        ScriptQueue_Exec(second);
        ScriptQueue_Drain();
        ScriptQueue_Drain();
        CHECK(!first->ResultDropped && first->ResultInt == 100);
        CHECK(!second->ResultDropped && second->ResultInt == 0);
        delete first;
        delete second;
        printf("queue: ordered exec + result round trip ok\n");
        draining = 0;
        drainer.join();

        // bounded drop-oldest: SetCapacity clamps to >= 16; Exec BLOCKS the
        // pushing script thread, so the flood needs concurrent pushers (like
        // 24 concurrent scripts): 16 fill the queue and wait, the 8 beyond
        // capacity wake immediately as dropped
        constexpr int FLOOD_COUNT = 24;
        XA_SCRIPT_COMMAND *pushed[FLOOD_COUNT];
        std::thread pushers[FLOOD_COUNT];
        for (int i = 0; i < FLOOD_COUNT; i++)
        {
            pushed[i] = new XA_SCRIPT_COMMAND();
            pushed[i]->Tag = SC_NOW;
        }
        for (int i = 0; i < FLOOD_COUNT; i++)
        {
            pushers[i] = std::thread([i, &pushed]() {
                ScriptQueue_Exec(pushed[i], 5000);
            });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ScriptQueue_Drain(); // runs what is left, wakes the rest as dropped
        for (int i = 0; i < FLOOD_COUNT; i++)
        {
            pushers[i].join();
        }
        int dropped = 0;
        int delivered = 0;
        for (int i = 0; i < FLOOD_COUNT; i++)
        {
            if (pushed[i]->ResultDropped)
            {
                dropped++;
            }
            else
            {
                delivered++;
            }
            delete pushed[i];
        }
        CHECK(dropped > 0 && delivered > 0);
        printf("queue: drop-oldest under flood ok (dropped=%d delivered=%d)\n", dropped,
            delivered);

        // --- pointer marshaling (the >4GB truncation fix) -------------------
        // A live >4GB heap probe is not feasible here: the build disables
        // exceptions (-fno-exceptions), so a failed 5GB reserve aborts instead
        // of throwing bad_alloc. The static_assert in commandqueue.h pins the
        // slot width; here a real pointer still round-trips through the queue.
        {
            std::vector<char> big;
            big.reserve(4096);
            auto *command = new XA_SCRIPT_COMMAND();
            command->Tag = SC_NOW;
            command->PointerA = (void *)big.data();
            // Exec blocks, so the drain runs on a one-shot helper thread
            std::thread oneShot([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                ScriptQueue_Drain();
            });
            ScriptQueue_Exec(command);
            oneShot.join();
            CHECK(!command->ResultDropped);
            CHECK(command->PointerA == (void *)big.data());
            delete command;
            printf("pointer: slot round trip ok (%p)\n", (void *)big.data());
        }

        // --- result copyback (journal readers ride PointerA) -----------------
        // The real executor hands the found journal entry across the queue in
        // the pointer slot; complete_node must copy it back or every journal
        // read sees nullptr.
        {
            ScriptJournal_Add(CJournalMessage(0x1, 0x02B2, "poisoned"));
            auto *command = new XA_SCRIPT_COMMAND();
            command->Tag = SC_JOURNAL_FIND;
            command->TextA = "poisoned";
            command->TextD = "02B2"; // the color filter (0xFFFF = any)
            std::thread oneShot([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                ScriptQueue_Drain();
            });
            ScriptQueue_Exec(command);
            oneShot.join();
            CHECK(!command->ResultDropped);
            auto *entry = (CJournalMessage *)command->PointerA;
            CHECK(entry != nullptr && entry->Text == "poisoned");
            delete entry; // the script thread owns the handed-over copy
            delete command;
            ScriptJournal_Clear();
            printf("copyback: journal entry through PointerA ok\n");
        }

        // timeout: the caller record must read dropped, not real zeros
        {
            auto *slow = new XA_SCRIPT_COMMAND();
            slow->Tag = SC_NOW;
            ScriptQueue_Exec(slow, 50); // no drain is running here
            CHECK(slow->ResultDropped);
            CHECK(slow->ResultInt == 0);
            delete slow;
            printf("timeout: caller record marked dropped\n");
        }

        // --- the WaitFor* handshake (SC_EVENT_WAIT clear+poll semantics) ---
        // the REAL drain executor against the real g_GumpReceived flag: the
        // first execution clears a pre-set flag (g_GumpReceived =
        {
            auto *wait = new XA_SCRIPT_COMMAND();
            wait->Tag = SC_EVENT_WAIT;
            wait->SubTag = 2; // WF_GUMP (the apiscript flag order)
            for (int round = 0; round < 3; round++)
            {
                g_GumpReceived = true; // a gump "arrived" before the wait
                wait->ResultBool = false;
                std::thread drainOnce([]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(30));
                    ScriptQueue_Drain();
                });
                wait->BoolA = round > 0; // the caller-side armed marker
                ScriptQueue_Exec(wait);
                drainOnce.join();
                if (round == 0)
                {
                    CHECK(!wait->ResultDropped && !wait->ResultBool);
                    CHECK(!g_GumpReceived); // the drain cleared it
                }
                else if (round == 1)
                {
                    CHECK(wait->ResultBool); // the re-armed flag is visible
                }
            }
            delete wait;
            printf("wait: event-wait clear+poll handshake ok\n");
        }

        // shutdown: pending entries wake dropped, later pushes fail fast
        ScriptQueue_SetCapacity(64);
        auto *pending = new XA_SCRIPT_COMMAND();
        pending->Tag = SC_NOW;
        std::thread waiter([&]() { ScriptQueue_Exec(pending, 5000); });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ScriptQueue_Shutdown();
        waiter.join();
        CHECK(pending->ResultDropped);
        delete pending;

        auto *late = new XA_SCRIPT_COMMAND();
        late->Tag = SC_NOW;
        ScriptQueue_Exec(late);
        CHECK(late->ResultDropped);
        delete late;
        printf("queue: shutdown drain + fail-fast ok\n");
        ScriptQueue_Reset(); // section 6 re-arms the queue (test hook)
    }

    // --- 3. journal mirror ------------------------------------------------
    {
        CJournalMessage mine(0x1, 0x02B2, "You have been poisoned");
        mine.Flags = JMT_MY;
        CJournalMessage sys(0xFFFFFFFF, 0x03B2, "World is saving");
        sys.Flags = JMT_SYSTEM;
        ScriptJournal_Add(mine);
        ScriptJournal_Add(sys);

        CJournalMessage *hit = ScriptJournal_Find("poisoned", 0, 0xFFFF, 0, 0, 0, false);
        CHECK(hit != nullptr && hit->FindTextID == 0);
        delete hit;

        hit = ScriptJournal_Find("saving|poisoned", 0, 0xFFFF, 0, 0, 0, false);
        CHECK(hit != nullptr && hit->FindTextID == 0); // newest first
        delete hit;

        hit = ScriptJournal_Find("poisoned", 0, 0xFFFF, JMT_SYSTEM, 0, 0, false);
        CHECK(hit == nullptr); // type filter
        hit = ScriptJournal_Find("poisoned", 0, 0xFFFF, JMT_MY, 0, 0, true);
        CHECK(hit != nullptr);
        delete hit; // remove semantics: gone now
        hit = ScriptJournal_Find("poisoned", 0, 0xFFFF, 0, 0, 0, false);
        CHECK(hit == nullptr);
        printf("journal: pattern/flags/find-id/remove ok\n");

        ScriptJournal_Clear();
    }

    // --- 4. script state mirrors (globals / timers / buffs) ----------------
    {
        ScriptGlobal_Set("lootbag", "0x40001234");
        ScriptGlobal_Set("lootbag", "0x40005678"); // overwrite (original map semantics)
        CHECK(ScriptGlobal_Get("lootbag") == "0x40005678");
        CHECK(ScriptGlobal_Get("missing") == ""); // unset -> empty (parity with the original)
        ScriptGlobal_Set("", "ignored");          // empty name dropped
        ScriptGlobal_Clear();
        CHECK(ScriptGlobal_Get("lootbag") == "");

        g_Ticks = 1000;
        ScriptTimer_Set("t", 100); // stored: now - value
        g_Ticks = 1500;
        CHECK(ScriptTimer_Exists("t"));
        CHECK(ScriptTimer_Get("t") == 600); // value + elapsed
        CHECK(ScriptTimer_Get("nope") == -1);
        ScriptTimer_Remove("t");
        CHECK(!ScriptTimer_Exists("t"));
        ScriptTimer_Set("u", 0);
        ScriptTimer_Clear();
        CHECK(!ScriptTimer_Exists("u"));

        ScriptBuffs_Add(0x7542, 60000, "Mortal Strike");
        CHECK(ScriptBuffs_Exists("mortal strike"));    // case-insensitive
        CHECK(ScriptBuffs_Exists("  MORTAL STRIKE ")); // trimmed
        CHECK(ScriptBuffs_Exists("0x7542"));           // icon-text lookup
        CHECK(!ScriptBuffs_Exists("night sight"));
        ScriptBuffs_Remove(0x7542); // the event carries the icon only
        CHECK(!ScriptBuffs_Exists("mortal strike"));
        printf("state: globals/timers/buffs mirrors ok\n");
    }

    // --- 5. file engine sandbox -------------------------------------------
    {
        char rootTemplate[] = "/tmp/xuo-harness-XXXXXX";
        char *mk = mkdtemp(rootTemplate);
        CHECK(mk != nullptr);
        const astr_t root(mk);
        ScriptFile_SetSandboxRoot(root);

        // relative paths resolve under the root
        CHECK(ScriptFile_Sanitize("logs/out.txt", "t") == root + "/logs/out.txt");
        // "inside" absolute paths stay
        CHECK(ScriptFile_Sanitize(root + "/ok.txt", "t") == root + "/ok.txt");
        // normalization keeps you in: "./a/../b" -> "b"
        CHECK(ScriptFile_Sanitize("./x/../y.txt", "t") == root + "/y.txt");
        // escapes are blocked and logged
        CHECK(ScriptFile_Sanitize("../escape.txt", "t") == "");
        CHECK(ScriptFile_Sanitize("a/../../escape.txt", "t") == "");
        CHECK(ScriptFile_Sanitize("/etc/passwd", "t") == "");
        CHECK(ScriptFile_Sanitize(root + "/../sibling.txt", "t") == "");

        // write / read round trip through the object API
        XA_SCRIPT_FILE file;
        CHECK(ScriptFile_Open(&file, "line.txt", false));
        ScriptFile_WriteLine(&file, "first");
        ScriptFile_WriteLine(&file, "second");
        ScriptFile_Write(&file, "tail");
        ScriptFile_Close(&file);

        CHECK(ScriptFile_Open(&file, "line.txt", true)); // checkExists hit
        CHECK(ScriptFile_ReadLine(&file) == "first");
        CHECK(ScriptFile_Read(&file) == "second"); // >> reads one token
        ScriptFile_Close(&file);

        // append re-opens without truncation
        CHECK(ScriptFile_Append(&file, "line.txt", false));
        ScriptFile_WriteLine(&file, "third");
        ScriptFile_Close(&file);
        CHECK(ScriptFile_Open(&file, "line.txt", true));
        CHECK(ScriptFile_ReadLine(&file) == "first");
        ScriptFile_Close(&file);

        // sandboxed remove + the missing-file parity
        CHECK(ScriptFile_RemovePath("line.txt"));
        CHECK(!ScriptFile_RemovePath("line.txt"));
        CHECK(!ScriptFile_Opened(&file));

        // the text window engine (scriptgump.cpp): ops, cap, save
        ScriptTextWindow_Command(TW_SET_POS, "", 30, 40, "");
        ScriptTextWindow_Command(TW_SET_SIZE, "", 320, 240, "");
        ScriptTextWindow_Command(TW_OPEN, "", 0, 0, "");
        CHECK(ScriptTextWindow_Opened());
        ScriptTextWindow_Command(TW_PRINT, "hello", 0, 0, "");
        ScriptTextWindow_Command(TW_PRINT, "world", 0, 0, "");
        const vector<astr_t> &lines = ScriptTextWindow_Lines();
        CHECK(lines.size() == 2 && lines[0] == "hello" && lines[1] == "world");
        ScriptTextWindow_Command(TW_CLEAR, "", 0, 0, "");
        CHECK(ScriptTextWindow_Lines().empty());
        for (int i = 0; i < 600; i++) // runaway print guard (512 cap)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "line %d", i);
            ScriptTextWindow_Command(TW_PRINT, buf, 0, 0, "");
        }
        CHECK(ScriptTextWindow_Lines().size() == 512);
        CHECK(ScriptTextWindow_Lines().front() == "line 88"); // oldest dropped
        ScriptTextWindow_SaveToFile(root + "/window.txt");
        ScriptTextWindow_Command(TW_CLOSE, "", 0, 0, "");
        CHECK(!ScriptTextWindow_Opened());
        CHECK(!ScriptFile_Open(&file, "../outside/window.txt", false)); // sandboxed save target
        ScriptFile_Close(&file);

        // cleanup
        ScriptFile_SetSandboxRoot(root);
        ScriptFile_RemovePath(root + "/window.txt");
        rmdir(root.c_str());
        printf("file: sandbox escape blocked + text window ok\n");
    }


    // --- 6. the engine classes: REAL registration wiring ---
    {
        std::atomic<int> draining{ 1 };
        std::thread drainer([](std::atomic<int> *running) {
            while (running->load())
            {
                ScriptQueue_Drain();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }, &draining);

        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx = JS_NewContext(rt);
        Script_RegisterAPI(ctx, "harness");

        // the SC_GUMP_INFO executor deep-copies the manager record; the stub
        // manager hands it this one (harnessstubs.cpp). The executor is the
        // real one, so the world pointer + the command gates must read
        // "connected" for the GetGump path to run.
        CGameWorld world;
        g_World = &world;
        g_EnabledCommands = ~0ULL;
        GumpData source;
        source.Serial = 0x40000123;
        source.ID = 5;
        source.X = 10;
        source.Y = 20;
        g_HarnessGump = &source;

        // the aliased-class bug shape: every engine class aliased the GameObject
        // class, so the copies came back with the WRONG prototype and the
        // gameobject finalizer freed them as the wrong type. Each `typeof`
        // probe below must report the owning class's method.
        const char *script = "function probe() {"
            "var g = Client.GetGump(0);"
            "var f = Client.NewFile();"
            "var h = Client.CreateGumpHook('1');"
            "var m = Client.CreateClientMacro();"
            "return [typeof g, g === null ? 'null' : typeof g.Select,"
            " g === null ? 'null' : typeof g.Close,"
            " typeof f, f === null ? 'null' : typeof f.Open,"
            " f === null ? 'null' : typeof f.WriteLine,"
            " typeof h, h === null ? 'null' : typeof h.AddEntry,"
            " h === null ? 'null' : typeof h.AddCheck,"
            " typeof m, m === null ? 'null' : typeof m.AddAction];"
            "}\n";
        JSValue program = JS_Eval(
            ctx, script, strlen(script), "<script>", JS_EVAL_TYPE_GLOBAL);
        CHECK(!JS_IsException(program));
        JS_FreeValue(ctx, program);

        JSValue global = JS_GetGlobalObject(ctx);
        JSValue probe = JS_GetPropertyStr(ctx, global, "probe");
        CHECK(JS_IsFunction(ctx, probe));
        JSValue result = JS_Call(ctx, probe, JS_UNDEFINED, 0, nullptr);
        CHECK(JS_IsArray(result));
        static const char *const EXPECTED[] = { "object",   // g: the Gump class
            "function", // g.Select
            "function", // g.Close
            "object",   // f: the File class
            "function", // f.Open
            "function", // f.WriteLine
            "object",   // h: the GumpHook class
            "function", // h.AddEntry
            "function", // h.AddCheck
            "object",   // m: the ClientMacro class
            "function"  // m.AddAction
        };
        for (size_t i = 0; i < count_of(EXPECTED); i++)
        {
            JSValue item = JS_GetPropertyUint32(ctx, result, (uint32_t)i);
            const char *text = JS_ToCString(ctx, item);
            const bool ok = text != nullptr && strcmp(text, EXPECTED[i]) == 0;
            printf("api: probe[%zu] = %s (want %s)%s\n", i, text ? text : "?", EXPECTED[i],
                ok ? "" : "  <-- FAIL");
            CHECK(ok);
            JS_FreeCString(ctx, text);
            JS_FreeValue(ctx, item);
        }

        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, probe);
        JS_FreeValue(ctx, global);
        draining = 0;
        drainer.join();
        // the runtime teardown runs the class finalizers (the gump deep copy
        // is freed through GumpData's destructor now, not the GameObject one)
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        g_HarnessGump = nullptr;
        g_World = nullptr;
        g_EnabledCommands = 0;
        printf("api: engine-class prototypes + finalizers ok\n");
    }

    // --- O. INI config round trips (Options / Hotkeys / Map schemas) ----
    {
        // the config root is pinned explicitly to a temp dir
        // (CAssistantConfig::SetRootPathOverride) so the tests stay
        // side-effect free (the default root is the client appdata)
        char dirTemplate[] = "/tmp/xuoassist_config_XXXXXX";
        const char *mkd = mkdtemp(dirTemplate);
        CHECK(mkd != nullptr);
        if (mkd != nullptr)
        {
            CAssistantConfig::SetRootPathOverride(fs_path_from(mkd));

            // --- Options.ini: every main/display/hotkeys/scripts field ---
            {
                CAssistantConfig saved;
                for (int i = 0; i < CLBI_COUNT; i++)
                {
                    saved.SetOption(i, (i % 3) != 0);
                }
                saved.m_MessagesLevel = 2;
                saved.m_SearchItemsDistance = 5;
                saved.m_UseItemsDistance = 4;
                saved.m_OpenCorpsesDistance = 1;
                saved.m_CorpseKeepDelay = 12345;
                saved.m_WaitTargetsDelay = 777;
                saved.m_MoveItemsDelay = 333;
                saved.m_UseItemsDelay = 222;
                saved.m_WindowsOpacity = 80;
                saved.m_UseFontColor = true;
                saved.m_FontColor = "0x02B2";
                saved.m_UseCharactersFontColor = true;
                saved.m_CharactersFontColor = "0x0044";
                saved.m_AutoReconnect = true;
                saved.m_AutoReconnectSeconds = 45;
                saved.m_DoubleClickForOpenDoors = true;
                saved.m_DisplayEnabled = true;
                saved.m_DisplayTitleData = "UO - {name} (& more) <test> 'q'";
                saved.m_PassHotkeys = true;
                saved.m_CommandPrefix = "-";
                saved.m_SortScripts = true;
                saved.m_ScriptLastDir = "/tmp/scripts";
                // the writer persists the LIVE list (g_LoadedScripts): seed it
                g_LoadedScripts.Clear();
                g_LoadedScripts.Add("scripts/a.js", 11u);
                g_LoadedScripts.Add("/tmp/scripts/b.js", 22u);

                saved.SaveMainOptions();

                CAssistantConfig loaded;
                loaded.LoadMainOptions();
                bool ok = true;
                for (int i = 0; i < CLBI_COUNT; i++)
                {
                    ok = ok && loaded.GetOption(i) == ((i % 3) != 0);
                }
                ok = ok && loaded.m_MessagesLevel == 2;
                ok = ok && loaded.m_SearchItemsDistance == 5;
                ok = ok && loaded.m_UseItemsDistance == 4;
                ok = ok && loaded.m_OpenCorpsesDistance == 1;
                ok = ok && loaded.m_CorpseKeepDelay == 12345;
                ok = ok && loaded.m_WaitTargetsDelay == 777;
                ok = ok && loaded.m_MoveItemsDelay == 333;
                ok = ok && loaded.m_UseItemsDelay == 222;
                ok = ok && loaded.m_WindowsOpacity == 80;
                ok = ok && loaded.m_UseFontColor && loaded.m_FontColor == "0x02B2";
                ok = ok && loaded.m_UseCharactersFontColor;
                ok = ok && loaded.m_CharactersFontColor == "0x0044";
                ok = ok && loaded.m_AutoReconnect && loaded.m_AutoReconnectSeconds == 45;
                ok = ok && loaded.m_DoubleClickForOpenDoors;
                ok = ok && loaded.m_DisplayEnabled;
                ok = ok && loaded.m_DisplayTitleData == "UO - {name} (& more) <test> 'q'";
                ok = ok && loaded.m_PassHotkeys && loaded.m_CommandPrefix == "-";
                ok = ok && loaded.m_SortScripts;
                ok = ok && loaded.m_ScriptLastDir == "/tmp/scripts";
                // the loaded-scripts list round trips in order (the writer
                // emits loadedscripts=<count> + loadedscript<i>)
                ok = ok && loaded.m_LoadedScripts.size() == 2;
                if (ok)
                {
                    ok = loaded.m_LoadedScripts[0] == "scripts/a.js" &&
                         loaded.m_LoadedScripts[1] == "/tmp/scripts/b.js";
                }
                CHECK(ok);
                g_LoadedScripts.Clear();
            }

            // --- the loaded-scripts list: restore from the persisted paths ---
            {
                // the restore drains the pending paths on the first draw and
                // marks a missing file instead of dropping the entry
                char dir2[] = "/tmp/xuoassist_scripts_XXXXXX";
                const char *mkd2 = mkdtemp(dir2);
                CHECK(mkd2 != nullptr);
                const astr_t present = astr_t(dir2) + "/present.js";
                const astr_t missing = astr_t(dir2) + "/missing.js";
                FILE *f = fopen(present.c_str(), "wb");
                CHECK(f != nullptr);
                if (f != nullptr)
                {
                    fputs("Client.Info('hi');\n", f);
                    fclose(f);
                }
                g_LoadedScripts.Clear();
                g_LoadedScripts.SetPendingRestore({ present, missing });
                g_LoadedScripts.DrainPendingRestore();
                CHECK(g_LoadedScripts.Scripts().size() == 2);
                const CLoadedScript *first = g_LoadedScripts.Find(ScriptList_NormalizePath(present));
                const CLoadedScript *second = g_LoadedScripts.Find(ScriptList_NormalizePath(missing));
                CHECK(first != nullptr && first->bExists && first->ContentHash != 0);
                CHECK(second != nullptr && !second->bExists);
                // dedupe through the normalized key (./a/../a is the same file)
                const size_t before = g_LoadedScripts.Scripts().size();
                g_LoadedScripts.Add(astr_t(dir2) + "/./present.js", 0u);
                CHECK(g_LoadedScripts.Scripts().size() == before);
                // display name + save bookkeeping
                CHECK(ScriptList_DisplayName(present) == "present.js");
                g_LoadedScripts.NotifySaved(first->Key, "Client.Info('hi');\n");
                CHECK(g_LoadedScripts.Find(first->Key)->ContentHash ==
                      ScriptList_ContentHash("Client.Info('hi');\n"));
                g_LoadedScripts.Clear();
            }

            // --- Hotkeys.ini: three records, all fields, order preserved ---
            {
                CAssistantConfig saved;
                CHotkeyEntry a;
                a.Key = 65;
                a.Modifiers = 2;
                a.Type = HT_SCRIPT;
                a.Action = 0;
                a.ScriptOrMacroName = "mine.ini";
                a.ExternalCode = "";
                a.RunningMode = true;
                CHotkeyEntry b;
                b.Key = 67;
                b.Modifiers = 1;
                b.Type = HT_USE_OBJECT;
                b.Action = 7;
                b.ScriptOrMacroName = "";
                b.ExternalCode = "printf('x; y')"; // ';' survives (no inline comments)
                CHotkeyEntry c;
                c.Key = 27;
                c.Modifiers = 0;
                c.Type = HT_TOGGLE_HOTKEYS;
                c.Action = 0;
                saved.m_Hotkeys = { a, b, c };
                saved.SaveHotkeys();

                CAssistantConfig loaded;
                CHECK(loaded.LoadHotkeys());
                CHECK(loaded.m_Hotkeys.size() == 3);
                bool ok = loaded.m_Hotkeys.size() == 3;
                if (ok)
                {
                    const CHotkeyEntry &la = loaded.m_Hotkeys[0];
                    const CHotkeyEntry &lb = loaded.m_Hotkeys[1];
                    const CHotkeyEntry &lc = loaded.m_Hotkeys[2];
                    ok = la.Key == 65 && la.Modifiers == 2 && la.Type == HT_SCRIPT &&
                         la.ScriptOrMacroName == "mine.ini" && la.ExternalCode.empty() &&
                         la.RunningMode;
                    ok = ok && lb.Key == 67 && lb.Modifiers == 1 && lb.Type == HT_USE_OBJECT &&
                         lb.Action == 7 && lb.ScriptOrMacroName.empty() && !lb.RunningMode &&
                         lb.ExternalCode == "printf('x; y')";
                    ok = ok && lc.Key == 27 && lc.Modifiers == 0 && lc.Type == HT_TOGGLE_HOTKEYS &&
                         lc.Action == 0;
                }
                CHECK(ok);
            }

            // --- Map.ini: every CMapSettings field, byte-identical ---
            {
                CAssistantConfig().EnsureDirectories();
                CMapSettings saved = CMapSettings();
                saved.WindowX = 12;
                saved.WindowY = 34;
                saved.WindowWidth = 567;
                saved.WindowHeight = 890;
                saved.ShowStatics = true;
                saved.ApplyAltitude = true;
                saved.AltitudeOnly = false;
                saved.SmoothImage = true;
                saved.DisplayCoordinates = true;
                saved.ShowOnLogin = true;
                saved.ShowLastCorpse = true;
                saved.MarkDeadMobiles = true;
                saved.Opacity = 75;
                saved.RotateMap = true;
                saved.PlayerFontStyle = 1;
                saved.PlayerColor = 0x41A2B3C4;
                saved.PlayerShowHits = true;
                saved.PlayerShowMana = true;
                saved.PlayerShowStam = true;
                saved.PlayerShowName = true;
                saved.PartyEnabled = true;
                saved.PartyFontStyle = 2;
                saved.PartyColor = 0x50607080;
                saved.PartyShowHits = true;
                saved.PartyShowName = true;
                saved.GuildEnabled = true;
                saved.GuildFontStyle = 3;
                saved.GuildColor = 0x90A0B0C0;
                saved.GuildShowHits = true;
                saved.GuildShowName = true;
                saved.FriendsEnabled = true;
                saved.FriendsFontStyle = 4;
                saved.FriendsColor = 0xD0E0F000;
                saved.FriendsShowHits = true;
                saved.FriendsShowName = true;
                saved.EnemiesEnabled = true;
                saved.EnemiesFontStyle = 5;
                saved.EnemiesColor = 0x11223344;
                saved.EnemiesShowHits = true;
                saved.EnemiesShowName = true;
                saved.OtherEnabled = true;
                saved.OtherFontStyle = 6;
                saved.OtherColor = 0x55667788;
                saved.OtherShowHits = true;
                saved.OtherShowName = true;
                saved.Save();

                CMapSettings loaded = CMapSettings();
                loaded.Load();
                // value-initialised on both sides, so the padding bytes match
                CHECK(memcmp(&saved, &loaded, sizeof(CMapSettings)) == 0);
            }

            CAssistantConfig::SetRootPathOverride(fs_path());
            printf("config: INI round trips ok (Options, Hotkeys, Map)\n");
        }
    }

    // --- N. AtomicFileWrite (the crash-safe save path) -------------------
    {
        // a temp dir keeps the test side-effect free
        char dirTemplate[] = "/tmp/xuoassist_atomic_XXXXXX";
        const char *mkd = mkdtemp(dirTemplate);
        CHECK(mkd != nullptr);
        if (mkd != nullptr)
        {
            const astr_t dir(mkd);

            // 1) create
            const fs_path path = fs_path_join(dir, "probe.bin");
            CHECK(AtomicFileWrite(path, "<a/>", 4));
            CHECK(fs_path_type(path) == FS_FILE);

            // 2) replace (the content is fully overwritten, no .tmp left)
            CHECK(AtomicFileWrite(path, "<bbbb/>", 7));
            {
                size_t size = 0;
                unsigned char *data = fs_map(path, &size);
                CHECK(data != nullptr && size == 7 && memcmp(data, "<bbbb/>", 7) == 0);
                fs_unmap(data, size);
            }
            CHECK(!fs_path_exists(fs_path_join(dir, "probe.bin.tmp")));

            // 3) failure leaves the target untouched (the directory is not
            const fs_path roDir = fs_path_join(dir, "ro");
            fs_path_create(roDir);
            chmod(fs_path_ascii(roDir), 0555);
            CHECK(!AtomicFileWrite(fs_path_join(roDir, "x.bin"), "<a/>", 4));
            chmod(fs_path_ascii(roDir), 0755);
            CHECK(!fs_path_exists(fs_path_join(roDir, "x.bin")));

            fs_del(fs_path_join(roDir, "x.bin"));
            rmdir(fs_path_ascii(roDir));
            fs_del(path);
            rmdir(mkd);
            printf("config: atomic save ok\n");
        }
    }

    // --- 9. core/textconv: ParseTextTokens parity + termination -----------
    // Byte parity with the old CTextParser (GetTokens/ReadTokens): the old
    // parser bounded each line at the first embedded NUL (GetEOL), quoted
    // tokens carry a trailing NUL inside the string (resize(size+1)), and
    // every input must terminate - the 2026-09 display-title freeze was
    // "{name\0}" (a stage-one quoted token) stalling the scan on the NUL.
    {
        // the display-tag stage two call: "name" out of "{name}"
        vector<astr_t> tags = ParseTextTokens("{name}", " \t{}=", "", "\"\"''");
        CHECK(tags.size() == 1 && tags[0] == "name");

        // the reported freeze input: the stage-one token "{name\0}" fed back
        // through CompileData; the old parser returned {"name"} (truncated
        // at the NUL) and terminated - so must the new one
        const astr_t stageOne("{name}", 6);
        vector<astr_t> quoted = ParseTextTokens(stageOne, "", "", "{}", false, true);
        CHECK(quoted.size() == 1);
        CHECK(quoted[0].size() == 7); // '{' + "name" + NUL quirk + '}'
        CHECK(quoted[0][5] == '\0' && quoted[0][6] == '}');
        vector<astr_t> stageTwo = ParseTextTokens(quoted[0], " \t{}=", "", "\"\"''");
        CHECK(stageTwo.size() == 1 && stageTwo[0] == "name");

        // quoted round trips (saveQuotes on/off), old parser parity
        vector<astr_t> say = ParseTextTokens("say \"a b\"", " ", "#", "\"\"''", true, true);
        CHECK(say.size() == 2 && say[0] == "say");
        CHECK(say[1].size() == 6 && say[1][0] == '"' && say[1][4] == '\0' && say[1][5] == '"');
        vector<astr_t> bare = ParseTextTokens("say \"a b\"", " ", "#", "\"\"''");
        CHECK(bare.size() == 2 && bare[0] == "say" && bare[1] == astr_t("a b", 4));

        // the old GetTokens took a const char*, so content after an embedded
        // NUL never made it into the tokens - truncation parity
        const astr_t mid("ab\0cd", 5);
        vector<astr_t> trunc = ParseTextTokens(mid, " ", "", "\"\"''");
        CHECK(trunc.size() == 1 && trunc[0] == "ab");

        // degenerate inputs terminate with the old parser's (empty) result
        const astr_t nuls(3, '\0');
        CHECK(ParseTextTokens(nuls, " ", "", "\"\"''").empty());
        CHECK(ParseTextTokens("", " ", "", "\"\"''").empty());
        // lone close quote / unmatched open quote must consume and finish
        CHECK(ParseTextTokens("'", " ", "", "\"\"''").empty());
        CHECK(ParseTextTokens("\"abc", " ", "", "\"\"''").size() == 1);

        printf("textconv: tokenizer parity ok\n");
    }

    // --- the script text-command table (Client.UseObject etc.) -------------
    // Every Client.* script call is marshaled as a text command executed by
    // CTextCommandManager::RunCommand. The table was never populated (nothing
    // called InitCommands), so ALL of them answered "Unknown command:
    // useobject" - the regression this covers. core/textcommandmanager.cpp is
    // linked for real (not stubbed) exactly for this test.
    {
        // nothing has consumed the table yet in the harness: this is the
        // state the client was in (0 = the "Unknown command" bug)
        const size_t before = CTextCommandManager::CommandCount();
        g_TextCommandManager.EnsureCommands();
        const size_t count = CTextCommandManager::CommandCount();
        printf("commands: %u before EnsureCommands, %u after\n", (unsigned)before,
            (unsigned)count);
        CHECK(count > 30); // the COMMAND() rows in textcommandmanager.cpp
        CHECK(CTextCommandManager::CommandCount() == count); // idempotent
        // the exact call Client.UseObject marshals
        CHECK(g_TextCommandManager.HasCommand("useobject"));
        CHECK(g_TextCommandManager.HasCommand("UseObject")); // case-insensitive
        printf("commands: %d registered (useobject included)\n", (int)count);
    }

    // --- forced stop: every tight-loop shape must terminate ----------------
    // "Stop" has to work at ANY moment for ANY script: a JS-only loop is
    // preempted by the QuickJS interrupt handler, a loop hammering the host
    // API must not starve it either.
    {
        CScriptManager &manager = *ScriptManager_Get();
        std::atomic<bool> draining{ true };
        std::thread drainThread([&draining]() {
            while (draining.load())
            {
                ScriptQueue_Drain();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
        struct LoopCase
        {
            const char *path;
            const char *source;
        };
        const LoopCase cases[] = {
            { "stop_a.js", "while (true) {}" },
            { "stop_b.js", "for (;;) {}" },
            { "stop_c.js", "var x = 0; while (1) { x = x + 1; }" },
            { "stop_d.js", "function f() { while (true) {} } f();" },
            { "stop_e.js", "while (true) { Client.Info('spam'); }" },
            { "stop_f.js", "for (var i = 0; ; i++) { var s = 'x'.repeat(10); }" },
            // native-getter loop: no mutation, only queued reads (the Player.*
            // getters are native FUNCTIONS: reading the property is pure JS,
            // calling it is the queued read)
            { "stop_g.js", "while (true) { var n = Player.Name(); var s = Player.Serial(); }" },
            // getter + mutation mix
            { "stop_h.js", "while (true) { Client.Wait(0); Player.Hits; }" },
            // a stopped script must not keep executing its body
            // a stopped script must stop PRINTING too: the counter freezes
            { "stop_i.js", "while (true) { Client.Info('STOPCOUNT'); }" },
            // a script must not be able to SWALLOW a Stop: the error is
            // uncatchable, so neither loop can continue after the catch
            { "stop_j.js",
                "while (true) { try { Client.Info('x'); } catch (e) {} }" },
            { "stop_k.js",
                "try { while (true) { Client.Info('x'); } } catch (e) {} "
                "while (true) { Client.Info('x'); }" },
        };
        for (const LoopCase &loopCase : cases)
        {
            manager.RunFile(loopCase.path, loopCase.source);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            CHECK(manager.RunningFile(loopCase.path));
            // sanity for the API-driven cases: the loop really is pushing
            // commands (a pure-JS loop never queues, so it has nothing to
            // check here - the drained check below still applies)
            const bool apiDriven = std::string(loopCase.path) == "stop_e.js" ||
                                   std::string(loopCase.path) == "stop_g.js" ||
                                   std::string(loopCase.path) == "stop_h.js" ||
                                   std::string(loopCase.path) == "stop_i.js";
            bool pumping = false;
            for (int i = 0; i < 50 && apiDriven && !pumping; i++)
            {
                pumping = ScriptQueue_Pending() > 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            if (apiDriven)
            {
                CHECK(pumping);
            }
            manager.TerminateFile(loopCase.path);
            bool stopped = false;
            for (int i = 0; i < 300 && !stopped; i++) // up to 3 s
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                stopped = !manager.RunningFile(loopCase.path);
            }
            // the script body must not run another iteration after the Stop:
            // it is gone from the running set AND it stopped enqueueing
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            const bool stillStopped = !manager.RunningFile(loopCase.path);
            const bool drained = ScriptQueue_Pending() == 0;
            printf("stop: %-10s '%.38s' %s (pending after stop: %u)\n", loopCase.path,
                loopCase.source, stopped && stillStopped && drained ? "ok" : "FAILED",
                ScriptQueue_Pending());
            CHECK(stopped);
            CHECK(stillStopped);
            CHECK(drained);
            manager.Tick();
        }
        draining = false;
        drainThread.join();
    }

    // --- run/terminate lifecycle (the real CScriptHandle flow) -------------
    // a top-level loop script starts, runs, survives a mid-flight terminate
    // request by exiting with "interrupted", and leaves the manager's list
    {
        CScriptManager &manager = *ScriptManager_Get();

        // the queued-command drain: Wait/Gump/etc exec calls resolve only
        // when the queue is drained (the client drains per frame; the
        // harness owns its own drain thread here)
        std::atomic<bool> draining{ true };
        std::thread drainThread([&draining]() {
            while (draining.load())
            {
                ScriptQueue_Drain();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
        struct DrainStop
        {
            std::atomic<bool> &flag;
            std::thread &thread;
            ~DrainStop()
            {
                flag = false;
                thread.join();
            }
        } drainStop{ draining, drainThread };

        // pure JS tight loop: only the interrupt handler can stop this. The
        // body is TOP-LEVEL: a script is its file, and only a function named
        // after the file is invoked (a top-level loop runs during Evaluate)
        manager.RunFile("loop.js", "while (true) {}");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        CHECK(manager.RunningFile("loop.js"));
        manager.TerminateFile("loop.js");
        bool stoppedPure = false;
        for (int i = 0; i < 100 && !stoppedPure; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            stoppedPure = !manager.RunningFile("loop.js");
        }
        CHECK(stoppedPure);
        if (!stoppedPure)
        {
            asm volatile("int3"); // debugger stop: dump all threads here
        }
        printf("lifecycle: pure JS loop terminate ok\n");

        // host-call loop: Wait spends most time in native sleeps. The loop
        // must be TOP-LEVEL and the body inlined on the SAME line or in a
        // plain block: the RBE 'm_Waiting' path only polls
        // TerminateRequested between statements.
        manager.RunFile("waiter.js", "for (;;) { Client.Wait('100ms'); }");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        CHECK(manager.RunningFile("waiter.js"));

        manager.TerminateFile("waiter.js");

        bool stopped = false;
        for (int i = 0; i < 100 && !stopped; i++) // up to 2s
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            stopped = !manager.RunningFile("waiter.js");
            if (i == 50 && !stopped)
            {
                Info(Assistant, "lifecycle: still running");
                for (CScriptHandle *handle : manager.Handles())
                {
                    Info(Assistant, "lifecycle:   handle '%s' state=%d", handle->Path().c_str(),
                         (int)handle->State());
                }
            }
        }
        CHECK(stopped);

        manager.Tick(); // the finished handle leaves the manager's list
        bool listed = false;
        for (CScriptHandle *handle : manager.Handles())
        {
            if (handle->Name() == "selftest")
            {
                listed = true;
            }
        }
        CHECK(!listed);
        printf("lifecycle: run/terminate ok\n");
    }

    if (failures == 0)
    {
        printf("ALL HARNESS TESTS PASSED\n");
        return 0;
    }
    printf("%d FAILURES\n", failures);
    return 1;
}
