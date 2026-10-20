// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The script text editor widget: a single-purpose JS code editor.
// - fast hand-written JS tokenizer (no regex): keywords, the assistant API
//   objects, member calls, sentinels, strings with escapes, template
//   literals, numbers, line and block comments (/* */ carries across lines),
//   all per-line cached;
// - auto-completion fed by the real script API surface
//   (ScriptAPI_CompletionList): members after `Client.`-style scopes,
//   global identifiers while typing, Ctrl+Space to force; mouse and keyboard
//   driven, inserts Name() with the caret between the parens;
// - mouse: click places the caret, drag selects, shift+click extends,
//   double-click selects a word, triple-click a line, IBeam cursor;
// - editing: multi-line clipboard (Ctrl+C/X/V), select all, redo,
//   auto-indent on Enter, snapshot undo, line-number gutter, Find/Replace/
//   GoToLine popups.
//
// Widget state is keyed by the owning buffer pointer (every document owns a
// stable astr_t), so SyncBuffer/release work from any window scope.

#include "../core/global.h"

#include <external/gfx/imgui/imgui.h>

namespace xuoassist {

// Re-splits the widget's line buffer from `buffer` after an external replace
// (the document keeps its astr_t; only the contents changed).
void TextEditor_SyncBuffer(astr_t *buffer);

// Drops the widget state for `buffer` (the document is going away).
void TextEditor_ReleaseState(astr_t *buffer);

// Draws the editor; returns true when the buffer was modified this frame.
// size 0 = fill the remaining window space. lineOffset/gotoRequest let the
// owner (scripteditor) drive GoToLine.
bool TextEditor_Draw(
    const char *id, astr_t *buffer, float height = 0.0f, int gotoLine = 0, bool *gotoPending = nullptr);

// Inserts text at the cursor (the command-insert menu target).
void TextEditor_InsertAtCursor(const astr_t &text);

// The current selection comment/uncomment helper (the toolbar
// Comment-selection action): returns the edited range or false.
bool TextEditor_CommentSelection();

// Text operations (the textoperationdialog popups drive these):
// find = case-sensitive search from the cursor, up/down; replace = replace at
// cursor then find next. All return true when something matched.
bool TextEditor_Find(const char *text, bool down, bool caseSensitive = true);
bool TextEditor_Replace(const char *findText, const char *replaceText, bool caseSensitive = true);
bool TextEditor_ReplaceAll(const char *findText, const char *replaceText, bool caseSensitive = true);
int TextEditor_CurrentLine();

} // namespace xuoassist
