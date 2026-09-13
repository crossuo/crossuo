// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef XUOASSIST_SCRIPT_SCRIPTHANDLE_H
#define XUOASSIST_SCRIPT_SCRIPTHANDLE_H

// The per-script worker thread + the script manager (was CScriptHandle +
// the running-scripts list bookkeeping of TabScripts, script/scripthandle.*:
// one QThread per script joined through the finished signal).
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
    CScriptHandle(const string &functionName, const vector<string> &args, const string &scriptBody);
    ~CScriptHandle();

    const string &Name() const { return m_FunctionName; }
    XA_SCRIPT_STATE State() const { return (XA_SCRIPT_STATE)m_State.load(); }

    bool GetPaused() const { return m_Paused; }
    void SetPaused(bool value);

    void Terminate();
    // Spawns the worker thread (the manager calls this after the cap check).
    void Start();
    // Joins the thread with a bounded wait; the caller never blocks forever.
    void Join(uint timeoutMs);

private:
    void RunEngine(); // the CScriptHandle::RunEngine port

    string m_FunctionName;
    vector<string> m_Args;
    string m_Text;

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
    CScriptHandle *Run(const string &functionName, const string &scriptBody,
        const vector<string> &args = vector<string>());

    void Tick(); // drops finished handles (the slot_RemoveRunningScriptItem flow)
    void StopAll(uint joinTimeoutMs = 2000);

    bool Running(const string &functionName) const;
    const vector<CScriptHandle *> &Handles() const { return m_Handles; }

private:
    vector<CScriptHandle *> m_Handles;
};

constexpr int SCRIPT_MAX_CONCURRENT = 16;

// The manager singleton (the tab + the API terminate/exec hooks use these).
CScriptManager *ScriptManager_Get();
void ScriptManager_Terminate(const string &functionName, const string &functionsSave);
bool ScriptManager_Running(const string &functionName);
void ScriptManager_Exec(const string &functionName, bool once, const vector<string> &args);
// The tab's LoadScript(path) / editor reload path. MAIN THREAD ONLY: script
// workers reach it marshaled through the queue (SC_EDITOR_LOAD) so the shared
// buffer write never races a frame.
void ScriptManager_LoadScriptFile(const string &path);
// The editor text bridge (the tab and the editor window share one buffer,
// like g_ScriptEditorDialog did).
string &ScriptEditor_Text();
string ScriptEditor_Path();
void ScriptEditor_SetPath(const string &path);
// Replaces the shared buffer on the caller's behalf AND bumps the buffer
// generation (the editor re-splits on the change: the stale-buffer fix).
void ScriptEditor_SetText(const string &text);
// The buffer-generation token: every external buffer replace bumps it; the
// editor widget compares and forces a re-split when it moved.
uint ScriptEditor_Generation();
vector<string> ScriptEditor_FunctionList(); // the `function NAME(` parse

} // namespace xuoassist

#endif // XUOASSIST_SCRIPT_SCRIPTHANDLE_H
