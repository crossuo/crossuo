// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The script editors: one ImGui window per open document, any number at once.
// Documents are created by the Scripts tab (double-click a loaded script), by
// "LoadScript" from the runtime and by the Macros tab ("To script",
// untitled). Each document owns its text and its UI state (find/replace/goto,
// status line, command menu) - nothing is shared between two editors.
//
// The run controls live in the Scripts tab: the editor edits, the tab runs.

#include "../core/global.h"

namespace xuoassist {

struct CEditorDoc
{
    astr_t Path;                 // empty = untitled
    astr_t Key;                  // normalized path (scriptlist key)
    astr_t Name;                 // window title label
    astr_t Text;                 // the working copy the widget edits in place
    bool Dirty{ false };
    bool Open{ true };           // window visibility flag (the X button)
    bool SaveAsFailedOnce{ false }; // the same-file probe guard (scriptpicker)

    // per-window UI state (never shared with another document)
    astr_t Status{ "Ready" };
    astr_t FindText;
    astr_t ReplaceText;
    int GotoLine{ 1 };
    bool GotoPending{ false };
};

// Opens `path` (reads the file). A document already open for the same file is
// focused instead of duplicated. Returns false when the file cannot be read.
bool Editors_Open(const astr_t &path);

// A fresh untitled document (no path yet; Save / Save As picks one).
void Editors_OpenUntitled(const astr_t &initialText);

// An untitled document with an explicit base name (the macro converter names
// its output; a serial suffix keeps repeated conversions distinct).
void Editors_OpenUntitled(const astr_t &baseName, const astr_t &initialText);

void Editors_CloseAll();

// Lookup by normalized key; nullptr when no editor has that file open.
CEditorDoc *Editors_Find(const astr_t &pathKey);

// The tab's "Reload from disk": replaces the document's text from the file.
bool Editors_ReloadFromDisk(const astr_t &path);

// Save As committed: rebind the document to `newPath` (title/key/clean) and
// import it into the loaded-scripts list.
void Editors_CommitSaveAs(CEditorDoc *doc, const astr_t &newPath);
// Save / disk reload committed: refresh the stored content hash.
void Editors_CommitSave(CEditorDoc *doc);
// Re-splits the editor widget after an external text replace.
void Editors_ResyncWidget(CEditorDoc *doc);

// Draws every open document window (called by the assistant shell while the
// assistant is visible).
void Editors_Draw();

} // namespace xuoassist
