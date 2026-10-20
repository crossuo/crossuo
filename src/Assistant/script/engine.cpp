// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The QuickJS-ng wrapper (see engine.h). QuickJS API notes kept from the
// upstream headers: JS_NewRuntime/JS_NewContext own everything below them;
// JS_FreeRuntime must be the LAST call. The exception -> journal reporting
// mirrors CScriptHandle::RunEngine's `result.isError()` branches:
// title + "Line: N\n" + description.

#include "engine.h"
#include "apiscript.h"
#include "commandqueue.h"

#include "../core/assist.h"

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

extern "C" {
#include "quickjs/quickjs.h"
}

#if defined(_WIN32)
#include <malloc.h> // alloca
#else
#include <alloca.h>
#endif
#include <atomic>
#include <chrono>
#include <thread>

namespace xuoassist {

namespace {

// The ScriptError surface, marshaled: g_Assistant.ScriptError lands in
// ClientPrint -> CreateUnicodeTextMessage - main-thread client state - so the
// lines ride SC_SAY (SAY_ERROR, no flood throttle) through the queue instead
// of running here on the script worker. The log line stays local: logging is
// cross-thread safe (and survives a dropped/shut-down queue).
void script_error(const astr_t &title, const astr_t &text)
{
    Warning(Assistant, "script error: %s %s\n", title.c_str(), text.c_str());
    auto *command = new XA_SCRIPT_COMMAND();
    command->Tag = SC_SAY;
    command->SubTag = SAY_ERROR;
    command->TextB = title;
    command->TextC = text;
    ScriptQueue_Exec(command);
    delete command;
}

// JS interrupt handler: runs on the script thread inside every JS loop. Two
// jobs: flip to "terminate" when the manager asked, and periodically pause
// the script (a tight JS loop never reaches an API call, so the QuickJS
// handler gives a hard preemption point).
// QuickJS contract: return non-zero when the JS code must be interrupted
// (zero keeps it running) - the returns below are the interrupted/continue
// pair, not stop/go
int ScriptInterruptHandler(JSRuntime *rt, void *opaque)
{
    auto *engine = (CScriptEngine *)opaque;
    if (engine == nullptr)
    {
        return 0;
    }
    if (engine->TerminateRequested())
    {
        // one line per interrupted script: names the victim so a stray
        // terminate request is traceable (the handle flag vs the engine flag)
        if (!engine->InterruptLogged)
        {
            engine->InterruptLogged = true;
            Info(Assistant, "script '%s' interrupted (terminate requested)",
                 engine->Name.c_str());
        }
        return 1; // stop execution with a pending "interrupted" exception
    }

    while (engine->PauseFlag() && !engine->TerminateRequested())
    {
        // same 10 ms cadence as CScriptEngineBase::PauseWaiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (engine->TerminateRequested())
    {
        return 1;
    }

    return 0;
}

} // namespace

CScriptEngine::CScriptEngine()
{
    m_Runtime = JS_NewRuntime();
    if (m_Runtime != nullptr)
    {
        JS_SetMemoryLimit(m_Runtime, SCRIPT_MEMORY_LIMIT);
        JS_SetInterruptHandler(m_Runtime, ScriptInterruptHandler, this);
        m_Context = JS_NewContext(m_Runtime);
    }
}

CScriptEngine::~CScriptEngine()
{
    if (m_Context != nullptr)
    {
        // JS_FreeContext runs no user code; a terminate requested during the
        // free is honored by the interrupt handler before this point.
        JS_FreeContext(m_Context);
        m_Context = nullptr;
    }
    if (m_Runtime != nullptr)
    {
        JS_FreeRuntime(m_Runtime);
        m_Runtime = nullptr;
    }
}

bool CScriptEngine::Setup(const astr_t &scriptName)
{
    if (m_Context == nullptr)
    {
        return false;
    }
    Name = scriptName;
    // the context opaque is the engine instance: every host native and the
    // pause/terminate sleep loops read it back (a null opaque made Wait's
    // terminate check a no-op, so a terminated script slept out its delays)
    JS_SetContextOpaque(m_Context, this);
    Script_RegisterAPI(m_Context, scriptName); // apiscript.cpp
    return true;
}

bool CScriptEngine::Evaluate(const astr_t &text)
{
    if (m_Context == nullptr)
    {
        return false;
    }
    // JS_EVAL_TYPE_GLOBAL: the
    // file's `function ...` declarations land on the global object.
    JSValue result = JS_Eval(
        m_Context, text.c_str(), text.length(), "<script>", JS_EVAL_TYPE_GLOBAL);
    bool ok = !JS_IsException(result);
    if (!ok)
    {
        astr_t line;
        astr_t message;
        Script_FormatException(m_Context, line, message); // apiscript.cpp
        script_error("Evaluate script error!", "Line: " + line + "\n" + message);
    }
    JS_FreeValue(m_Context, result);
    return ok;
}

bool CScriptEngine::HasFunction(const astr_t &functionName)
{
    if (m_Context == nullptr)
    {
        return false;
    }
    JSValue global = JS_GetGlobalObject(m_Context);
    JSValue func = JS_GetPropertyStr(m_Context, global, functionName.c_str());
    const bool ok = !JS_IsUndefined(func) && JS_IsFunction(m_Context, func);
    JS_FreeValue(m_Context, func);
    JS_FreeValue(m_Context, global);
    return ok;
}

bool CScriptEngine::CallFunction(const astr_t &functionName, const vector<astr_t> &args)
{
    if (m_Context == nullptr)
    {
        return false;
    }

    JSValue global = JS_GetGlobalObject(m_Context);
    JSValue func = JS_GetPropertyStr(m_Context, global, functionName.c_str());
    JS_FreeValue(m_Context, global);

    if (JS_IsUndefined(func) || !JS_IsFunction(m_Context, func))
    {
        JS_FreeValue(m_Context, func);
        // Original behavior: calling a missing function produced a TypeError from
        // the engine call; here the manager reports the same condition.
        script_error(
            "Running script error!", "Line: 0\nfunction '" + functionName + "' not found");
        return false;
    }

    const int count = (int)args.size();
    JSValue *argv = count != 0 ? (JSValue *)alloca(sizeof(JSValue) * count) : nullptr;
    for (int i = 0; i < count; i++)
    {
        argv[i] = JS_NewStringLen(m_Context, args[i].c_str(), args[i].length());
    }

    JSValue result = JS_Call(m_Context, func, JS_UNDEFINED, count, argv);
    JS_FreeValue(m_Context, func);
    for (int i = 0; i < count; i++)
    {
        JS_FreeValue(m_Context, argv[i]);
    }

    const bool ok = !JS_IsException(result);
    if (!ok)
    {
        astr_t line;
        astr_t message;
        Script_FormatException(m_Context, line, message);
        script_error("Running script error!", "Line: " + line + "\n" + message);
    }
    JS_FreeValue(m_Context, result);
    return ok;
}

void CScriptEngine::RequestTerminate()
{
    m_TerminateFlag = 1;
    Terminated = true; // the flag the Wait/Pause loops watched
}

} // namespace xuoassist
