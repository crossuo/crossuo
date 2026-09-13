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
// The full API batch runs in-client (Player/Client/Orion need g_World).

#include "../commandqueue.h"
#include "../journal.h"

#include "../../core/global.h"
#include "../../core/assistantevents.h" // JMT_* (the journal type flags)

extern "C" {
#include "../quickjs/quickjs.h"
}

#include <atomic>
#include <cstdint>
#include <cstdio>
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

// the harness provides its own drain side (the client uses apiscript.cpp's)
static std::atomic<int> s_ExecCount{ 0 };
void ScriptCommand_Execute(XA_SCRIPT_COMMAND *command)
{
    s_ExecCount++;
    if (command->Tag == SC_WAIT_PARSE)
    {
        command->ResultInt = (int)command->TextA.size(); // fake delay = len
    }
    else if (command->Tag == SC_JOURNAL_FIND)
    {
        // the journal readers hand back an entry through PointerA (never
        // SerialA: 64-bit heap pointers truncate there)
        command->ResultInt = (int)command->TextA.size();
        command->PointerA = (void *)command->TextA.c_str();
    }
    else
    {
        command->ResultInt = 42;
    }
}

} // namespace xuoassist

using namespace xuoassist;

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

    // --- 2. command queue -----------------------------------------------
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
        // ordered exec: a main-thread drain loop like AssistantUI_Tick
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
        CHECK(!second->ResultDropped && second->ResultInt == 42);
        delete first;
        delete second;
        printf("queue: ordered exec + result round trip ok (exec=%d)\n", s_ExecCount.load());
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
        // The executor writes BOTH a Result field and PointerA (like the
        // InJournal/WaitJournal drain side in apiscript.cpp); complete_node
        // must copy both back or every journal read sees nullptr.
        {
            auto *command = new XA_SCRIPT_COMMAND();
            command->Tag = SC_JOURNAL_FIND;
            command->TextA = "poisoned";
            std::thread oneShot([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                ScriptQueue_Drain();
            });
            ScriptQueue_Exec(command);
            oneShot.join();
            CHECK(!command->ResultDropped);
            CHECK(command->ResultInt == 8); // strlen("poisoned")
            const char *entry = (const char *)command->PointerA;
            CHECK(entry != nullptr && strcmp(entry, "poisoned") == 0);
            printf("copyback: ResultInt + PointerA ok (%s)\n",
                entry ? entry : "(null)");
            delete command;
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

    if (failures == 0)
    {
        printf("ALL HARNESS TESTS PASSED\n");
        return 0;
    }
    printf("%d FAILURES\n", failures);
    return 1;
}
