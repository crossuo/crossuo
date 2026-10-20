// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The Scripts tab's loaded-scripts list: one entry per imported script file.
// The tab imports (Load), runs (Play/Pause/Stop) and opens editors for these
// entries; the list itself persists through CAssistantConfig (Options.ini
// [scripts] loadedscript0..N) and is restored on the next start.
//
// Identity: a script IS its file. The run manager keys its handles by the
// normalized path (scripthandle.h), so Play/Pause/Stop address one file and
// the same file never runs twice.

#include "../core/global.h"

namespace xuoassist {

struct CLoadedScript
{
    astr_t Path;                 // as loaded (user-facing, displayed)
    astr_t Key;                  // normalized path (identity, dedupe, run key)
    uint32_t ContentHash{ 0 };   // xxh32 of the bytes read at load; 0 = unknown
    bool bExists{ true };        // the path existed at the last Load/Restore
};

class CLoadedScriptList
{
public:
    const vector<CLoadedScript> &Scripts() const { return m_Scripts; }

    bool Contains(const astr_t &key) const;
    const CLoadedScript *Find(const astr_t &key) const;
    // Adds (or refreshes) an entry; the path is normalized for its Key.
    // Returns the stable Key the manager uses as the run identity.
    astr_t Add(const astr_t &path, uint32_t hash, bool exists = true);
    void Remove(const astr_t &key);
    void Clear();

    // Restore hand-off: LoadMainOptions queues the persisted paths, the first
    // TabScripts_Draw drains them (the config loads before the UI exists and
    // the existence check touches the filesystem once per entry).
    void SetPendingRestore(const vector<astr_t> &paths);
    void DrainPendingRestore();

    // Save As bookkeeping: refresh the stored content hash.
    void NotifySaved(const astr_t &key, const astr_t &text);

    // The FIRST entry whose content hash matches (the desktop save dialog's
    // same-file heuristic probe); nullptr when none.
    const CLoadedScript *FindByHash(uint32_t hash) const;

private:
    vector<CLoadedScript> m_Scripts;
    vector<astr_t> m_PendingRestore;
};

extern CLoadedScriptList g_LoadedScripts;

// --- helpers ------------------------------------------------------------------

// Reads the whole file; false when it cannot be opened.
bool ScriptList_ReadFile(const astr_t &path, astr_t &out);
// Crash-safe write (temp + rename, the AtomicFileWrite path).
bool ScriptList_WriteFile(const astr_t &path, const astr_t &text);

uint32_t ScriptList_ContentHash(const astr_t &text);

// Lexical normalization: collapses //, ./ and a/../b; keeps relative paths
// relative and absolute paths absolute. Windows: lowercased (case-insensitive
// compare). No filesystem access (Save As targets may not exist yet).
astr_t ScriptList_NormalizePath(const astr_t &path);
// The file name only (row label / editor window title).
astr_t ScriptList_DisplayName(const astr_t &path);
// The parent directory (save dialogs default to it).
astr_t ScriptList_DirName(const astr_t &path);

} // namespace xuoassist
