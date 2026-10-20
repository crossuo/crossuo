// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See scripthandle.h. The thread body is the CScriptEngine flow: build the
// engine, evaluate the whole file (a script IS its file: top-level code is the
// script), report errors through g_Assistant.ScriptError; finished rows leave
// the manager on Tick.

#include "scripthandle.h"
#include "apiscript.h" // Script_ReportInterrupted (the forced-stop error)
#include "commandqueue.h" // ScriptQueue_SetCancelHook (the forced-stop hook)
#include "engine.h"
#include "scriptlist.h"

#include "../core/assist.h"
#include "../core/assistantevents.h" // g_AssistantEvents (Script not found)

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include <algorithm>
#include <atomic>

namespace xuoassist {

// --- the function scan ---------------------------------------------------------

// The GetFunctionList port (scripteditordialog.cpp): scan for
// `function NAME(` declarations, trimmed.
vector<astr_t> Script_AnalyzeFunctions(const astr_t &text)
{
    vector<astr_t> list;
    size_t pos = 0;
    while ((pos = text.find("function", pos)) != astr_t::npos)
    {
        // word boundary before
        const bool boundary = pos == 0 || !isalnum((unsigned char)text[pos - 1]);
        if (boundary)
        {
            size_t start = pos + 8;
            while (start < text.size() && isspace((unsigned char)text[start]))
            {
                start++;
            }
            size_t end = start;
            while (end < text.size() && (isalnum((unsigned char)text[end]) || text[end] == '_'))
            {
                end++;
            }
            size_t paren = text.find('(', end);
            if (end > start && paren != astr_t::npos &&
                text.find_first_not_of(" \t", end) == paren)
            {
                list.push_back(text.substr(start, end - start));
            }
        }
        pos += 8;
    }
    return list;
}

// --- CScriptHandle ------------------------------------------------------------

namespace {

// The queue's cancel hook on the worker thread: the engine flag OR the owning
// handle's terminate flag (Terminate sets the latter first).
bool engine_cancelled(void *opaque)
{
    auto *engine = (CScriptEngine *)opaque;
    return engine == nullptr || engine->TerminateRequested();
}

// The aborted-wait notification: raise the uncatchable "interrupted" error on
// this script's context (apiscript.cpp owns the error shape).
void engine_abort(void *opaque)
{
    Script_ReportInterrupted((CScriptEngine *)opaque);
}

} // namespace

CScriptHandle::CScriptHandle(
    const astr_t &path, const vector<astr_t> &args, const astr_t &scriptBody)
    : m_Path(path)
    , m_Name(ScriptList_DisplayName(path))
    , m_Args(args)
    , m_Text(scriptBody)
{
}

CScriptHandle::~CScriptHandle()
{
    Terminate();
    Join(1000);
}

void CScriptHandle::SetPaused(bool value)
{
    m_Paused = value ? 1 : 0;
    CScriptEngine *engine = m_Engine.load();
    if (engine != nullptr)
    {
        engine->SetPausedFlag(value);
    }
}

void CScriptHandle::Terminate()
{
    Info(Assistant, "script terminate requested: %s\n", m_Path.c_str());
    // Set the handle flag FIRST: a worker that has not assigned m_Engine yet
    // still honors it as its first action (and the engine picks it up through
    // the wired external flag). The engine-level interrupt below stays the
    // fast path once the engine exists.
    m_TerminatePending = 1;
    CScriptEngine *engine = m_Engine.load();
    if (engine != nullptr)
    {
        engine->RequestTerminate();
        // a paused script must fall out of its pause loops too: the handle
        // cleared m_Paused, but the live engine flag gates the host-side
        // sleeps (PauseGate/Wait) independently
        engine->SetPausedFlag(false);
    }
    m_Paused = 0;
}

void CScriptHandle::Join(uint32_t timeoutMs)
{
    if (!m_Thread.joinable())
    {
        return;
    }
    const uint32_t start = g_Ticks;
    // bounded wait: poll the state (std::thread has no timed join). The
    // worker ends in SS_FINISHED/SS_FAILED (RunEngine); SS_ZOMBIE is only
    // set by the timeout path below - all three mean the thread is done
    // (or refusing to die), so join it instead of burning the timeout.
    while (m_State != SS_FINISHED && m_State != SS_FAILED && m_State != SS_ZOMBIE)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if ((uint32_t)(g_Ticks - start) > timeoutMs)
        {
            m_State = SS_ZOMBIE;
            m_Thread.detach(); // never hang the client on a stuck script
            Info(Assistant, "script thread '%s' did not stop within %u ms; detached",
                m_Path.c_str(), timeoutMs);
            return;
        }
    }
    if (m_Thread.joinable())
    {
        m_Thread.join();
    }
}

void CScriptHandle::Start()
{
    CScriptHandle *self = this;
    // a bigger stack for the worker: the engine object lives on it and
    // QuickJS' exception unwinding is stack-hungry under load
    m_Thread = std::thread([self]() {
        self->RunEngine();
    });
}

// The original ctor started the thread immediately; here the manager constructs and
// spawns in one step (Run above) so the cap is checked before the spawn.
void CScriptHandle::RunEngine()
{
    // first action: a terminate that raced the start kills the run before
    // any engine work happens
    if (m_TerminatePending.load() != 0)
    {
        m_State = SS_FINISHED;
        return;
    }

    CScriptEngine engine;
    m_Engine = &engine;
    // the handle flag doubles as the engine's external terminate: the
    // interrupt handler and the pause/sleep paths poll TerminateRequested()
    engine.SetExternalTerminateFlag(&m_TerminatePending);
    // Forced stop: every queued call this thread makes polls the same flag
    // while it waits, so Stop aborts an in-flight call even from a script
    // that never returns to bytecode (a native-call loop).
    ScriptQueue_SetCancelHook(engine_cancelled, engine_abort, &engine);
    if (!engine.Setup(m_Name))
    {
        m_State = SS_FAILED;
        m_Engine = nullptr;
        return;
    }

    if (!engine.Evaluate(m_Text))
    {
        m_State = SS_FAILED;
    }
    else if (engine.HasFunction(m_Name))
    {
        // a script file is a top-level script, but the historical shape
        // (and the converted macros) declare `function <file name>()`: call
        // it. Everything else has already run during Evaluate.
        if (!engine.CallFunction(m_Name, m_Args))
        {
            m_State = SS_FAILED;
        }
    }

    ScriptQueue_SetCancelHook(nullptr, nullptr, nullptr);
    m_Engine = nullptr;
    if (m_State != SS_FAILED)
    {
        m_State = SS_FINISHED;
    }
}

// --- CScriptManager -----------------------------------------------------------

CScriptManager *ScriptManager_Get()
{
    static CScriptManager manager;
    return &manager;
}

CScriptHandle *CScriptManager::RunFile(
    const astr_t &path, const astr_t &scriptBody, const vector<astr_t> &args)
{
    // one instance per file: an already running/paused script is returned
    // as-is (the Play button never stacks a second copy of the same file).
    // The key is normalized here so Play, the API and hand-built paths all
    // address the same file.
    const astr_t key = ScriptList_NormalizePath(path);
    for (size_t i = 0; i < m_Handles.size();)
    {
        CScriptHandle *handle = m_Handles[i];
        const XA_SCRIPT_STATE state = handle->State();
        if (handle->Path() == key &&
            (state == SS_FINISHED || state == SS_FAILED || state == SS_ZOMBIE))
        {
            // a finished run of this file still waiting for Tick: drop it now
            // so a re-Play between two ticks is not mistaken for a live run
            delete handle;
            m_Handles.erase(m_Handles.begin() + i);
            continue;
        }
        i++;
    }
    for (CScriptHandle *handle : m_Handles)
    {
        const XA_SCRIPT_STATE state = handle->State();
        if (handle->Path() == key && (state == SS_RUNNING || state == SS_PAUSED))
        {
            return handle;
        }
    }

    if ((int)m_Handles.size() >= SCRIPT_MAX_CONCURRENT)
    {
        g_Assistant.ClientPrint("Too many running scripts (max " +
                                str_from(SCRIPT_MAX_CONCURRENT) + ").");
        return nullptr;
    }

    auto *handle = new CScriptHandle(key, args, scriptBody);
    m_Handles.push_back(handle);
    handle->Start();
    return handle;
}

void CScriptManager::Tick()
{
    // remove finished rows
    m_Handles.erase(std::remove_if(m_Handles.begin(), m_Handles.end(),
                        [](CScriptHandle *handle) {
                            const XA_SCRIPT_STATE state = handle->State();
                            if (state == SS_FINISHED || state == SS_FAILED)
                            {
                                delete handle;
                                return true;
                            }
                            return false;
                        }),
        m_Handles.end());
}

void CScriptManager::StopAll(uint32_t joinTimeoutMs)
{
    for (CScriptHandle *handle : m_Handles)
    {
        handle->Terminate();
    }
    for (CScriptHandle *handle : m_Handles)
    {
        handle->Join(joinTimeoutMs);
    }
    // finished handles are collected by Tick; zombies stay visible
}

bool CScriptManager::RunningFile(const astr_t &path) const
{
    const astr_t key = ScriptList_NormalizePath(path);
    for (const CScriptHandle *handle : m_Handles)
    {
        const XA_SCRIPT_STATE state = handle->State();
        if (handle->Path() == key && (state == SS_RUNNING || state == SS_PAUSED))
        {
            return true;
        }
    }
    return false;
}

bool CScriptManager::PausedFile(const astr_t &path) const
{
    const astr_t key = ScriptList_NormalizePath(path);
    for (const CScriptHandle *handle : m_Handles)
    {
        if (handle->Path() == key && handle->State() == SS_PAUSED)
        {
            return true;
        }
    }
    return false;
}

void CScriptManager::TerminateFile(const astr_t &path)
{
    const astr_t key = ScriptList_NormalizePath(path);
    for (CScriptHandle *handle : m_Handles)
    {
        if (handle->Path() == key)
        {
            handle->Terminate();
        }
    }
}

bool CScriptManager::TogglePauseFile(const astr_t &path)
{
    const astr_t key = ScriptList_NormalizePath(path);
    bool any = false;
    for (CScriptHandle *handle : m_Handles)
    {
        const XA_SCRIPT_STATE state = handle->State();
        if (handle->Path() == key && (state == SS_RUNNING || state == SS_PAUSED))
        {
            handle->SetPaused(!handle->GetPaused());
            any = true;
        }
    }
    return any;
}

bool CScriptManager::RunningName(const astr_t &name) const
{
    for (const CScriptHandle *handle : m_Handles)
    {
        const XA_SCRIPT_STATE state = handle->State();
        if (handle->Name() == name && (state == SS_RUNNING || state == SS_PAUSED))
        {
            return true;
        }
    }
    return false;
}

void CScriptManager::TerminateName(const astr_t &name)
{
    for (CScriptHandle *handle : m_Handles)
    {
        if (handle->Name() == name)
        {
            handle->Terminate();
        }
    }
}

// --- the tab/hotkey entry points -----------------------------------------------------------------

CScriptHandle *ScriptManager_PlayFile(const astr_t &path)
{
    // a script IS its file: Play runs the file's CURRENT bytes from disk (an
    // open editor's unsaved changes do not participate - the tab tooltip
    // says so), whole file top-level
    astr_t text;
    if (!ScriptList_ReadFile(path, text))
    {
        g_AssistantEvents->OnClientPrint("Script not found: " + path);
        return nullptr;
    }
    return ScriptManager_Get()->RunFile(path, text);
}

bool ScriptManager_PausedFile(const astr_t &path)
{
    return ScriptManager_Get()->PausedFile(path);
}

bool ScriptManager_TogglePauseFile(const astr_t &path)
{
    return ScriptManager_Get()->TogglePauseFile(path);
}

void ScriptManager_TerminateFile(const astr_t &path)
{
    ScriptManager_Get()->TerminateFile(path);
}

bool ScriptManager_RunningFile(const astr_t &path)
{
    return ScriptManager_Get()->RunningFile(path);
}

// --- name-keyed compatibility surface ------------------------------------------------------------

void ScriptManager_Terminate(const astr_t &functionName, const astr_t &functionsSave)
{
    // the "all"/save-list semantics of the original TerminateScript; handles
    // are keyed by file now, so the match goes through the ScriptName (the
    // file name the runtime sees)
    CScriptManager *manager = ScriptManager_Get();
    if (str_lower(functionName) == "all")
    {
        for (CScriptHandle *handle : manager->Handles())
        {
            bool canTerminate = true;
            size_t start = 0;
            const astr_t lower = str_lower(functionsSave);
            while (start <= lower.size())
            {
                size_t end = lower.find('|', start);
                if (end == astr_t::npos)
                {
                    end = lower.size();
                }
                if (end > start && handle->Name() == lower.substr(start, end - start))
                {
                    canTerminate = false;
                    break;
                }
                start = end + 1;
            }
            if (canTerminate)
            {
                handle->Terminate();
            }
        }
        return;
    }

    manager->TerminateName(functionName);
    manager->TerminateFile(functionName); // also accept a loaded-scripts key
}

bool ScriptManager_Running(const astr_t &functionName)
{
    return ScriptManager_Get()->RunningName(functionName);
}

void ScriptManager_Exec(const astr_t &scriptName, bool once, const vector<astr_t> &args)
{
    if (once && ScriptManager_Running(scriptName))
    {
        return;
    }
    // Exec starts another script by name: resolve it among the loaded
    // scripts (the file whose body declares it), else treat the argument as
    // a path
    const vector<CLoadedScript> &scripts = g_LoadedScripts.Scripts();
    for (const CLoadedScript &script : scripts)
    {
        astr_t text;
        if (!ScriptList_ReadFile(script.Path, text))
        {
            continue;
        }
        const vector<astr_t> functions = Script_AnalyzeFunctions(text);
        if (std::find(functions.begin(), functions.end(), scriptName) != functions.end())
        {
            ScriptManager_Get()->RunFile(script.Key, text, args);
            return;
        }
    }
    ScriptManager_PlayFile(scriptName);
}

} // namespace xuoassist
