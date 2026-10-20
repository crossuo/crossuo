// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The per-script worker thread + the script manager: one worker thread per
// script file, identified by the normalized path (scriptlist.h). A script is
// its file: the worker evaluates the whole body top-level and calls a
// function named after the file when the body declares one (the historical
// shape, and what the macro converter emits).

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
    // `path` is the script file's identity (the normalized loaded-scripts key,
    // scriptlist.h): Play/Pause/Stop address it and the manager never starts
    // the same file twice.
    CScriptHandle(const astr_t &path, const vector<astr_t> &args, const astr_t &scriptBody);
    ~CScriptHandle();

    CScriptHandle(const CScriptHandle &) = delete;
    CScriptHandle &operator=(const CScriptHandle &) = delete;

    const astr_t &Path() const { return m_Path; }
    // The script-name the runtime sees (ScriptName): the file name.
    const astr_t &Name() const { return m_Name; }
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

    astr_t m_Path;
    astr_t m_Name;
    vector<astr_t> m_Args;
    astr_t m_Text;

    std::atomic<CScriptEngine *> m_Engine{ nullptr }; // Terminate reads it cross-thread
    std::atomic<int> m_TerminatePending{ 0 }; // survive a pre-engine terminate
    std::thread m_Thread;
    std::atomic<int> m_Paused{ 0 };
    std::atomic<int> m_State{ SS_RUNNING };
};

// The manager: owns the handles, feeds
class CScriptManager
{
public:
    CScriptManager() = default;
    CScriptManager(const CScriptManager &) = delete;
    CScriptManager &operator=(const CScriptManager &) = delete;
    // Runs `path`'s body as its own thread (whole-file top-level). An already
    // running/paused handle for the same path is returned instead of starting
    // a second one. Returns nullptr when the concurrent cap (16) is hit.
    CScriptHandle *RunFile(const astr_t &path, const astr_t &scriptBody,
        const vector<astr_t> &args = vector<astr_t>());

    void Tick(); // drops finished handles (the slot_RemoveRunningScriptItem flow)
    void StopAll(uint32_t joinTimeoutMs = 2000);

    bool RunningFile(const astr_t &path) const;
    bool PausedFile(const astr_t &path) const;
    void TerminateFile(const astr_t &path);
    bool TogglePauseFile(const astr_t &path);
    // Any handle whose ScriptName equals `name` (the Exec/Terminate script
    // API compatibility surface).
    bool RunningName(const astr_t &name) const;
    void TerminateName(const astr_t &name);
    const vector<CScriptHandle *> &Handles() const { return m_Handles; }

private:
    vector<CScriptHandle *> m_Handles;
};

constexpr int SCRIPT_MAX_CONCURRENT = 16;

// The manager singleton (the tab + the API terminate/exec hooks use these).
CScriptManager *ScriptManager_Get();
// The Scripts tab Play button / hotkey run: fresh file bytes, whole-file
// top-level, one instance per file.
CScriptHandle *ScriptManager_PlayFile(const astr_t &path);
bool ScriptManager_PausedFile(const astr_t &path);
bool ScriptManager_TogglePauseFile(const astr_t &path);
void ScriptManager_TerminateFile(const astr_t &path);
bool ScriptManager_RunningFile(const astr_t &path);
// Name-keyed forms (Exec/Terminate script APIs, the `terminate` text command):
// they match the running handles' ScriptName (the file name).
void ScriptManager_Terminate(const astr_t &functionName, const astr_t &functionsSave);
bool ScriptManager_Running(const astr_t &functionName);
void ScriptManager_Exec(const astr_t &scriptName, bool once, const vector<astr_t> &args);

// The `function NAME(` scan over a script text (the editor statusbar counts
// them; Exec resolves a name across the loaded scripts with it).
vector<astr_t> Script_AnalyzeFunctions(const astr_t &text);

} // namespace xuoassist
