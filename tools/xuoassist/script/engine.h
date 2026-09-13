// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef XUOASSIST_SCRIPT_ENGINE_H
#define XUOASSIST_SCRIPT_ENGINE_H

// The QuickJS-ng runtime wrapper (was the QScriptEngine usage inside
// CScriptHandle::RunEngine, scripthandle.cpp:120-180): one JSRuntime + one
// JSContext per running script, the assistant API surface registered as
// plain C natives (apiscript.cpp), the sentinel globals from the Qt engine
// (self/ground/lasttarget/... ) registered on the global object, and the
// evaluate/function-call flow with error reporting through the journal.
//
// The Qt globals were *strings* ("self", "lasttarget", ... - see
// scripthandle.cpp:154-165): the command layer pattern-matched those
// sentinels to the live serials. The same strings are registered here, so
// existing scripts keep working; helpers that receive them resolve them the
// same way (CAssistant::TextToSerial handles "self"/"backpack"/"lasttarget"
// etc. - core/assist.cpp, ported in Phase 1c).
//
// Interruption: JS_SetInterruptHandler polls the terminate flag (the
// handle-level atomic wired in through SetExternalTerminateFlag, plus the
// engine's own flag); Terminate flips it so even a `while(1){}` script
// returns to the worker, which then exits and the handle joins (plan rule:
// terminate must always work). Memory: 64 MB runtime malloc limit per script.

#include "../core/global.h"

#include <atomic>

struct JSRuntime;
struct JSContext;

namespace xuoassist {

constexpr size_t SCRIPT_MEMORY_LIMIT = 64 * 1024 * 1024; // per-script runtime

class CScriptEngine
{
    SETGET(bool, Paused, false)
    SETGET(bool, Terminated, false)
    SETGET(string, Name, "")

public:
    CScriptEngine();
    ~CScriptEngine();

    CScriptEngine(const CScriptEngine &) = delete;
    CScriptEngine &operator=(const CScriptEngine &) = delete;

    bool IsValid() const { return m_Context != nullptr; }

    // Registers the runtime API (Player/Client/Orion/TextWindow/SelectedTile
    // objects, the sentinel globals, ScriptName). Called by the ctor; kept
    // separate for clarity of the failure path.
    bool Setup(const string &scriptName);

    // The CScriptHandle::RunEngine flow: evaluate the whole file (defines the
    // functions), then call `functionName` with the string args (Qt behavior:
    // an evaluate error aborts before the call; both paths report through
    // g_Assistant.ScriptError exactly like signal_ScriptError did).
    // Returns false when the script produced an error (already reported).
    bool Evaluate(const string &text);
    bool CallFunction(const string &functionName, const vector<string> &args);

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

#endif // XUOASSIST_SCRIPT_ENGINE_H
