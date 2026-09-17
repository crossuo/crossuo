// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The script UI engine: the script TEXT WINDOW, the one UI-visible part of
// the script API.
// One singleton text window per process; a script that calls Open twice
// re-opens the same window. Rendered as a small ImGui window from
// AssistantUI_Draw. Caveat: AssistantUI_Draw only runs while the assistant
// shell is visible, so the window vanishes with the assistant hidden (the
// data stays alive - reopening shows the same lines).
// TextWindow* ops reach this module marshaled through the queue
// (SC_TEXT_WINDOW), so all state here is main-thread owned.
// SaveToFile writes the current lines (the sandbox rules of scriptfile.h
// apply).

#include "../core/global.h"

namespace xuoassist {

// SC_TEXT_WINDOW ops (SubTag). `ScriptTextWindow_Command` is the drain-side
// dispatcher; the natives build the command record.
enum XA_TEXT_WINDOW_OP
{
    TW_OPEN = 0,
    TW_CLOSE,
    TW_CLEAR,
    TW_PRINT,   // TextA
    TW_SET_POS, // IntA, IntB
    TW_SET_SIZE, // IntA, IntB
    TW_SAVE     // TextA = path
};

// Main-thread ops (run inside the queue drain).
void ScriptTextWindow_Command(
    int op, const astr_t &text, int intA, int intB, const astr_t &path);
// The TW_SAVE body (kept public for tests).
void ScriptTextWindow_SaveToFile(const astr_t &path);

// ImGui rendering of the window (defined in ui/assistantui.cpp next to the
// other ImGui code; AssistantUI_Draw calls it). No-op while the assistant
// shell is closed or the window was never opened.
void ScriptTextWindow_Draw();

// The current line snapshot for tests (main thread).
const vector<astr_t> &ScriptTextWindow_Lines();
bool ScriptTextWindow_Opened();
// The window geometry for the renderer (main thread; may be null).
void ScriptTextWindow_Rect(int *x, int *y, int *width, int *height);

} // namespace xuoassist
