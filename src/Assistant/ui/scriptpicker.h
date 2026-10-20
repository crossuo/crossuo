// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The script file pickers - the only place that opens a dialog.
//
// Desktop: the native OS dialogs (tinyfiledialogs), synchronous.
// Web (XUO_EMSC): the client runs inside the emscripten pthread worker with
// PROXY_TO_PTHREAD, so the DOM and the page's FS are not reachable from C++.
// The page (resources/shell.html) owns both: globalThis.__xuoScriptPick
// imports the chosen files into /data/scripts (persisted with the rest of the
// page state) and reports the absolute paths back; a small export lets the
// worker read them. The browser dialog is asynchronous, so Load is a
// request/poll pair on both backends (the desktop one completes on the first
// poll).
//
// Headless runs never pick files: nothing calls into here without the UI.

#include "../core/global.h"

namespace xuoassist {

// --- Load (multi-select) ------------------------------------------------------

// Requests a picker. The result arrives through ScriptPicker_Poll on a later
// frame (the desktop backend completes inside this call).
void ScriptPicker_RequestLoad();

// Collects a finished pick. Returns true when `outPaths` was filled; false
// while the picker is still open or when nothing was picked.
bool ScriptPicker_Poll(vector<astr_t> &outPaths);

// --- Save As -------------------------------------------------------------------

// Synchronous: picks the target path only (the caller writes the file).
// Returns false when cancelled. `failedOnce` is the same-file probe guard: the
// desktop dialog considers a chosen file whose content equals the suggested
// one as "not picked"; when that happens `failedOnce` is flipped so the next
// press still resolves.
bool ScriptPicker_SaveAs(const astr_t &suggestedPath, astr_t &outPath, bool &failedOnce);

// The default directory for a new script on this backend:
// g_AssistantConfig.ScriptLastDir() on desktop, /data/scripts on the web.
astr_t ScriptPicker_DefaultDir();

// Joins a directory and a file name for the pickers' suggestion strings.
astr_t ScriptPicker_JoinPath(const astr_t &dir, const astr_t &name);

} // namespace xuoassist
