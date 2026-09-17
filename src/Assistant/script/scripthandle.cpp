// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See scripthandle.h. The thread body is the CScriptHandle::process +
// RunEngine port: build the engine, evaluate, call
// the function, report errors through g_Assistant.ScriptError, remove the
// running row on the way out.

#include "scripthandle.h"
#include "engine.h"

#include "../core/assist.h"

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include <algorithm>
#include <atomic>

namespace xuoassist {

// --- the shared editor buffer --------------------------------------------------

namespace {
astr_t s_EditorText;
astr_t s_EditorPath;
// The buffer-generation token: bumped by every external buffer replace
// (SetText/LoadScript); TextEditor_Draw re-splits when it moves. Without it
// the editor only re-split on the buffer POINTER changing - which never
// happens for the shared astr_t - so Open/New/Reload left stale lines and an
// edit rejoined the OLD content over the new file (data loss on save).
uint32_t s_EditorGeneration = 0;
} // namespace

astr_t &ScriptEditor_Text()
{
    return s_EditorText;
}

void ScriptEditor_SetText(const astr_t &text)
{
    s_EditorText = text;
    s_EditorGeneration++;
}

uint32_t ScriptEditor_Generation()
{
    return s_EditorGeneration;
}

astr_t ScriptEditor_Path()
{
    return s_EditorPath;
}

void ScriptEditor_SetPath(const astr_t &path)
{
    s_EditorPath = path;
}

// The GetFunctionList port (scripteditordialog.cpp): scan for
// `function NAME(` declarations, trimmed; the tab's start-combo feeds on it.
vector<astr_t> ScriptEditor_FunctionList()
{
    vector<astr_t> list;
    const astr_t &text = s_EditorText;
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

CScriptHandle::CScriptHandle(
    const astr_t &functionName, const vector<astr_t> &args, const astr_t &scriptBody)
    : m_FunctionName(functionName)
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
    // Set the handle flag FIRST: a worker that has not assigned m_Engine yet
    // still honors it as its first action (and the engine picks it up through
    // the wired external flag). The engine-level interrupt below stays the
    // fast path once the engine exists.
    m_TerminatePending = 1;
    CScriptEngine *engine = m_Engine.load();
    if (engine != nullptr)
    {
        engine->RequestTerminate();
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
    // bounded wait: poll the flag (std::thread has no timed join)
    while (m_State != SS_ZOMBIE)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if ((uint32_t)(g_Ticks - start) > timeoutMs)
        {
            m_State = SS_ZOMBIE;
            m_Thread.detach(); // never hang the client on a stuck script
            Info(Assistant, "script thread '%s' did not stop within %u ms; detached",
                m_FunctionName.c_str(), timeoutMs);
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
    if (!engine.Setup(m_FunctionName))
    {
        m_State = SS_FAILED;
        m_Engine = nullptr;
        return;
    }

    if (!engine.Evaluate(m_Text))
    {
        m_State = SS_FAILED;
    }
    else if (!m_FunctionName.empty() && !engine.CallFunction(m_FunctionName, m_Args))
    {
        m_State = SS_FAILED;
    }

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

CScriptHandle *CScriptManager::Run(
    const astr_t &functionName, const astr_t &scriptBody, const vector<astr_t> &args)
{
    if ((int)m_Handles.size() >= SCRIPT_MAX_CONCURRENT)
    {
        g_Assistant.ClientPrint("Too many running scripts (max " +
                                str_from(SCRIPT_MAX_CONCURRENT) + ").");
        return nullptr;
    }

    auto *handle = new CScriptHandle(functionName, args, scriptBody);
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

bool CScriptManager::Running(const astr_t &functionName) const
{
    for (const CScriptHandle *handle : m_Handles)
    {
        if (handle->Name() == functionName &&
            (handle->State() == SS_RUNNING || handle->State() == SS_PAUSED))
        {
            return true;
        }
    }
    return false;
}

// --- the API/tab entry points ---------------------------------------------------

void ScriptManager_Terminate(const astr_t &functionName, const astr_t &functionsSave)
{
    // the "all"/save-list semantics of TabScripts::TerminateScript
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

    for (CScriptHandle *handle : manager->Handles())
    {
        if (handle->Name() == functionName)
        {
            handle->Terminate();
        }
    }
}

bool ScriptManager_Running(const astr_t &functionName)
{
    return ScriptManager_Get()->Running(functionName);
}

void ScriptManager_Exec(const astr_t &functionName, bool once, const vector<astr_t> &args)
{
    if (once && ScriptManager_Running(functionName))
    {
        return;
    }
    // run the named function of the current editor text
    ScriptManager_Get()->Run(functionName, ScriptEditor_Text(), args);
}

void ScriptManager_LoadScriptFile(const astr_t &path)
{
    // load the script file into the editor buffer
    FILE *file = fopen(path.c_str(), "rb");
    if (file == nullptr)
    {
        return;
    }
    astr_t text;
    char buffer[4096];
    size_t read = 0;
    while ((read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        text.append(buffer, read);
    }
    fclose(file);
    ScriptEditor_SetText(text); // bumps the editor buffer generation
    s_EditorPath = path;
}

} // namespace xuoassist
