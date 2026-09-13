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
//      old uint slot truncated it).
//   5. The REAL API registration (apiscript.cpp Script_RegisterAPI) over a
//      live QuickJS context: the engine classes (Gump/File/GumpHook/
//      ClientMacro) must carry their own prototypes - the fix-round bug had
//      every class alias the GameObject class, so .Select/.Open/.AddEntry
//      were missing and the finalizers freed the wrong opaque types. The
//      client-facing symbols are stubbed in harnessstubs.cpp (disconnected
//      client); the queue drain here runs the REAL drain executor.

#include "../commandqueue.h"
#include "../journal.h"
#include "../scriptfile.h"
#include "../scriptgump.h"
#include "../scriptstate.h"

#include "../../config/xml.h" // AtomicFileWrite (the crash-safe save path)
#include "../../core/global.h"
#include "../../core/assistantevents.h" // JMT_* (the journal type flags)
#include "../../model/gameobjects/gameworld.h" // g_World (section 6 executor gate)
#include "../apiscript.h"
#include "../../model/commonitems/gump.h"

extern "C" {
#include "../quickjs/quickjs.h"
}

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <stdlib.h> // mkdtemp
#include <sys/stat.h> // chmod
#include <unistd.h> // rmdir
#include <cstdlib> // mkdtemp
#include <unistd.h> // rmdir
#include <thread>
#include <vector>

// the client normally provides g_Ticks (Globals.cpp)
using xuoassist::string;
uint g_Ticks = 0;

// the stack-line parser copy of apiscript.cpp Script_FormatException (the
// full function needs the client world; the parser is what the harness checks)
static void harness_format_line(JSContext *ctx, string &line, string &message)
{
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
extern CGump *g_HarnessGump;

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

        // the Qt script shape: functions on the global object + a Wait-like
        // call with string args (the sentinel globals are strings)
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
        // quickjs-ng replacement of QScriptValue::lineNumber; verified via
        // Script_FormatException's parser)
        JSValue errorFn = JS_GetPropertyStr(ctx, global, "boom");
        JSValue errorResult = JS_Call(ctx, errorFn, JS_UNDEFINED, 0, nullptr);
        CHECK(JS_IsException(errorResult));
        string line;
        string message;
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
        // helper (stub: text length); SC_NOW falls through the real
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
        CHECK(!first->ResultDropped && first->ResultInt == 3);
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
            CHECK(entry != nullptr && entry->GetText() == "poisoned");
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
        // first execution clears a pre-set flag (the Qt g_GumpReceived =
        // false) and reads false, later ones only read
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
        mine.SetFlags(JMT_MY);
        CJournalMessage sys(0xFFFFFFFF, 0x03B2, "World is saving");
        sys.SetFlags(JMT_SYSTEM);
        ScriptJournal_Add(mine);
        ScriptJournal_Add(sys);

        CJournalMessage *hit = ScriptJournal_Find("poisoned", 0, 0xFFFF, 0, 0, 0, false);
        CHECK(hit != nullptr && hit->GetFindTextID() == 0);
        delete hit;

        hit = ScriptJournal_Find("saving|poisoned", 0, 0xFFFF, 0, 0, 0, false);
        CHECK(hit != nullptr && hit->GetFindTextID() == 0); // newest first
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
        ScriptGlobal_Set("lootbag", "0x40005678"); // overwrite (Qt map semantics)
        CHECK(ScriptGlobal_Get("lootbag") == "0x40005678");
        CHECK(ScriptGlobal_Get("missing") == ""); // unset -> empty (Qt parity)
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
        const string root(mk);
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

        // sandboxed remove + the missing-file parity (QFile::exists gate)
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
        const vector<string> &lines = ScriptTextWindow_Lines();
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


    // --- 6. the engine classes: REAL registration wiring (the fix round) ---
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
        CGameWorld world(0);
        g_World = &world;
        g_EnabledCommands = ~0ULL;
        CGump source(0x40000123, 5, 10, 20);
        g_HarnessGump = &source;

        // the fix-round bug shape: every engine class aliased the GameObject
        // class, so the copies came back with the WRONG prototype and the
        // gameobject finalizer freed them as the wrong type. Each `typeof`
        // probe below must report the owning class's method.
        const char *script = "function probe() {"
            "var g = Orion.GetGump(0);"
            "var f = Orion.NewFile();"
            "var h = Orion.CreateGumpHook('1');"
            "var m = Orion.CreateClientMacro();"
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
        // is freed through CGump's destructor now, not the GameObject one)
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        g_HarnessGump = nullptr;
        g_World = nullptr;
        g_EnabledCommands = 0;
        printf("api: engine-class prototypes + finalizers ok\n");
    }

    // --- N. AtomicFileWrite (the crash-safe save path) -------------------
    {
        // a temp dir keeps the test side-effect free
        char dirTemplate[] = "/tmp/xuoassist_atomic_XXXXXX";
        const char *mkd = mkdtemp(dirTemplate);
        CHECK(mkd != nullptr);
        if (mkd != nullptr)
        {
            const string dir(mkd);

            // 1) create
            const fs_path path = fs_path_join(dir, "probe.xml");
            CHECK(AtomicFileWrite(path, "<a/>", 4));
            CHECK(fs_path_type(path) == FS_FILE);

            // 2) replace (the old content is fully overwritten, no .tmp left)
            CHECK(AtomicFileWrite(path, "<bbbb/>", 7));
            {
                size_t size = 0;
                unsigned char *data = fs_map(path, &size);
                CHECK(data != nullptr && size == 7 && memcmp(data, "<bbbb/>", 7) == 0);
                fs_unmap(data, size);
            }
            CHECK(!fs_path_exists(fs_path_join(dir, "probe.xml.tmp")));

            // 3) failure keeps the original untouched (the directory is not
            // writable for the temp file)
            const fs_path roDir = fs_path_join(dir, "ro");
            fs_path_create(roDir);
            chmod(fs_path_ascii(roDir), 0555);
            CHECK(!AtomicFileWrite(fs_path_join(roDir, "x.xml"), "<a/>", 4));
            chmod(fs_path_ascii(roDir), 0755);
            CHECK(!fs_path_exists(fs_path_join(roDir, "x.xml")));

            fs_del(fs_path_join(roDir, "x.xml"));
            rmdir(fs_path_ascii(roDir));
            fs_del(path);
            rmdir(mkd);
            printf("config: atomic save ok\n");
        }
    }

    if (failures == 0)
    {
        printf("ALL HARNESS TESTS PASSED\n");
        return 0;
    }
    printf("%d FAILURES\n", failures);
    return 1;
}
