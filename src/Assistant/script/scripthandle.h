// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The per-script worker thread + the script manager (was CScriptHandle +
// the running-scripts list bookkeeping of TabScripts: one worker thread
// per script joined through the finished signal).
//
// Lifecycle: Start() spawns the thread (capped at 16 concurrent), the thread
// builds a CScriptEngine, evaluates the file, calls the function, and exits.
// Terminate() flips the HANDLE's atomic flag first - a worker killed before it
// assigns m_Engine still sees it as its first action, and the engine sees it
// through the wired-in external flag (the engine-level interrupt stays the
// fast path for QuickJS preemption); Join with a timeout - a thread stuck in
// native code is detached and marked zombie (never block the main thread,
// plan rule). Pause mirrors CScriptEngineBase::PauseWaiting.

#include "../core/global.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace xuoassist {

class CScriptEngine;

enum XA_SCRIPT_STATE
{
    SS_RUNNING = 0,
    SS_PAUSED,
    SS_FINISHED,
    SS_FAILED, // the evaluate/call reported an error
    SS_ZOMBIE  // the thread refused to die; detached
};

class CScriptHandle
{
public:
    CScriptHandle(const astr_t &functionName, const vector<astr_t> &args, const astr_t &scriptBody);
    ~CScriptHandle();

    const astr_t &Name() const { return m_FunctionName; }
    XA_SCRIPT_STATE State() const { return (XA_SCRIPT_STATE)m_State.load(); }

    bool GetPaused() const { return m_Paused; }
    void SetPaused(bool value);

    void Terminate();
    // Spawns the worker thread (the manager calls this after the cap check).
    void Start();
    // Joins the thread with a bounded wait; the caller never blocks forever.
    void Join(uint32_t timeoutMs);

private:
    void RunEngine(); // the CScriptHandle::RunEngine port

    astr_t m_FunctionName;
    vector<astr_t> m_Args;
    astr_t m_Text;

    std::atomic<CScriptEngine *> m_Engine{ nullptr }; // Terminate reads it cross-thread
    std::atomic<int> m_TerminatePending{ 0 }; // survive a pre-engine terminate
    std::thread m_Thread;
    std::atomic<int> m_Paused{ 0 };
    std::atomic<int> m_State{ SS_RUNNING };
};

// The manager (was the running list of TabScripts): owns the handles, feeds
// the Scripts tab rows. All calls main-thread.
class CScriptManager
{
public:
    // Runs `functionName` from `scriptBody` as its own thread. Returns the
    // handle or nullptr when the concurrent cap (16) is hit.
    CScriptHandle *Run(const astr_t &functionName, const astr_t &scriptBody,
        const vector<astr_t> &args = vector<astr_t>());

    void Tick(); // drops finished handles (the slot_RemoveRunningScriptItem flow)
    void StopAll(uint32_t joinTimeoutMs = 2000);

    bool Running(const astr_t &functionName) const;
    const vector<CScriptHandle *> &Handles() const { return m_Handles; }

private:
    vector<CScriptHandle *> m_Handles;
};

constexpr int SCRIPT_MAX_CONCURRENT = 16;

// The manager singleton (the tab + the API terminate/exec hooks use these).
CScriptManager *ScriptManager_Get();
void ScriptManager_Terminate(const astr_t &functionName, const astr_t &functionsSave);
bool ScriptManager_Running(const astr_t &functionName);
void ScriptManager_Exec(const astr_t &functionName, bool once, const vector<astr_t> &args);
// The tab's LoadScript(path) / editor reload path. MAIN THREAD ONLY: script
// workers reach it marshaled through the queue (SC_EDITOR_LOAD) so the shared
// buffer write never races a frame.
void ScriptManager_LoadScriptFile(const astr_t &path);
// The editor text bridge (the tab and the editor window share one buffer,
// like g_ScriptEditorDialog did).
astr_t &ScriptEditor_Text();
astr_t ScriptEditor_Path();
void ScriptEditor_SetPath(const astr_t &path);
// Replaces the shared buffer on the caller's behalf AND bumps the buffer
// generation (the editor re-splits on the change: the stale-buffer fix).
void ScriptEditor_SetText(const astr_t &text);
// The buffer-generation token: every external buffer replace bumps it; the
// editor widget compares and forces a re-split when it moved.
uint32_t ScriptEditor_Generation();
vector<astr_t> ScriptEditor_FunctionList(); // the `function NAME(` parse

} // namespace xuoassist
