// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The script text editor widget (was CScriptPlainTextEdit + the Qt
// CSyntaxHighlighter + CExtendedPlainTextEdit gutter, plan §3.2): line
// buffer, tokenizer highlighting with the Qt palette (.Method dark-magenta,
// keywords dark-blue, globals dark-yellow, numbers/0x-hex blue, strings
// dark-green, // comments gray), line-number gutter, current-line highlight,
// a snapshot undo stack and the Find/Replace/GoToLine popups.
//
// Frame rules (plan): no whole-buffer rehighlight - only visible lines are
// tokenized per frame, results cached per line hash; no per-frame heap churn
// beyond the visible-line runs vector.

#include "../core/global.h"

#include <external/gfx/imgui/imgui.h>

namespace xuoassist {

// Draws the editor; returns true when the buffer was modified this frame.
// size 0 = fill the remaining window space. lineOffset/gotoRequest let the
// owner (scripteditor) drive GoToLine.
bool TextEditor_Draw(
    const char *id, string *buffer, float height = 0.0f, int gotoLine = 0, bool *gotoPending = nullptr);

// Inserts text at the cursor (the command-insert menu target).
void TextEditor_InsertAtCursor(const string &text);

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

