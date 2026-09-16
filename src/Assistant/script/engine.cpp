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

#include <alloca.h>
#include <atomic>
#include <chrono>
#include <thread>

namespace xuoassist {

namespace {

// JS interrupt handler: runs on the script thread inside every JS loop. Two
// jobs: flip to "terminate" when the manager asked, and periodically pause
// the script (the original PauseWaiting while-loop ran between API calls; a tight
// JS loop never reached one, QuickJS' handler gives us a hard preemption
// point the original engine never had).
int ScriptInterruptHandler(JSRuntime *rt, void *opaque)
{
    auto *engine = (CScriptEngine *)opaque;
    if (engine == nullptr)
    {
        return 1;
    }

    if (engine->TerminateRequested())
    {
        return 0; // stop execution with a pending "interrupted" exception
    }

    while (engine->PauseFlag() && !engine->TerminateRequested())
    {
        // same 10 ms cadence as CScriptEngineBase::PauseWaiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (engine->TerminateRequested())
    {
        return 0;
    }

    return 1;
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
    Script_RegisterAPI(m_Context, scriptName); // apiscript.cpp
    return true;
}

bool CScriptEngine::Evaluate(const astr_t &text)
{
    if (m_Context == nullptr)
    {
        return false;
    }
    // JS_EVAL_TYPE_GLOBAL matches the old script engine's evaluate semantics (the
    // file's `function ...` declarations land on the global object).
    JSValue result = JS_Eval(
        m_Context, text.c_str(), text.length(), "<script>", JS_EVAL_TYPE_GLOBAL);
    bool ok = !JS_IsException(result);
    if (!ok)
    {
        astr_t line;
        astr_t message;
        Script_FormatException(m_Context, line, message); // apiscript.cpp
        g_Assistant.ScriptError("Evaluate script error!", "Line: " + line + "\n" + message);
    }
    JS_FreeValue(m_Context, result);
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
        g_Assistant.ScriptError(
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
        g_Assistant.ScriptError("Running script error!", "Line: " + line + "\n" + message);
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
