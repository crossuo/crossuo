// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The QuickJS-ng runtime wrapper inside CScriptHandle::RunEngine: one
// JSRuntime + one JSContext per running script, the assistant API surface
// registered as plain C natives (apiscript.cpp), the sentinel globals
// (self/ground/lasttarget/... ) registered on the global object, and the
// evaluate/function-call flow with error reporting through the journal.
//
// Interruption: JS_SetInterruptHandler polls the terminate flag (the
// handle-level atomic wired in through SetExternalTerminateFlag, plus the
// engine's own flag); Terminate flips it so even a `while(1){}` script
// returns to the worker, which then exits and the handle joins.
// Memory: 64 MB runtime malloc limit per script.

#include "../core/global.h"

#include <atomic>

struct JSRuntime;
struct JSContext;

namespace xuoassist {

constexpr size_t SCRIPT_MEMORY_LIMIT = 64 * 1024 * 1024; // per-script runtime

class CScriptEngine
{
public:
    bool Paused = false;
    bool Terminated = false;
    astr_t Name = "";

    CScriptEngine();
    ~CScriptEngine();

    CScriptEngine(const CScriptEngine &) = delete;
    CScriptEngine &operator=(const CScriptEngine &) = delete;

    bool IsValid() const { return m_Context != nullptr; }

    // Registers the runtime API (Player/Client/TextWindow/SelectedTile
    // objects, the sentinel globals, ScriptName). Called by the ctor; kept
    // separate for clarity of the failure path.
    bool Setup(const astr_t &scriptName);

    // The CScriptHandle::RunEngine flow: evaluate the whole file (defines the
    // functions), then call `functionName` with the astr_t args (original behavior:
    // an evaluate error aborts before the call; both paths report through
    // g_Assistant.ScriptError).
    bool Evaluate(const astr_t &text);
    bool CallFunction(const astr_t &functionName, const vector<astr_t> &args);

    // Interrupt plumbing (the worker thread and the manager set/read these).
    void RequestTerminate();
    bool TerminateRequested() const
    {
        return m_TerminateFlag != 0 ||
            (m_ExternalTerminate != nullptr && m_ExternalTerminate->load() != 0);
    }
    // Wires the owning handle's terminate flag in (scripthandle.cpp): a
    // Terminate that arrives before/during the run is seen by the interrupt
    // handler and the pause/sleep paths even though the engine-level flag was
    // never set directly. The engine flag stays the fast path.
    void SetExternalTerminateFlag(const std::atomic<int> *flag)
    {
        m_ExternalTerminate = flag;
    }
    void SetPausedFlag(bool value) { m_PauseFlag = value; }
    bool PauseFlag() const { return m_PauseFlag; }

private:
    JSRuntime *m_Runtime{ nullptr };
    JSContext *m_Context{ nullptr };
    std::atomic<int> m_TerminateFlag{ 0 };
    std::atomic<int> m_PauseFlag{ 0 };
    const std::atomic<int> *m_ExternalTerminate{ nullptr };
};

} // namespace xuoassist
