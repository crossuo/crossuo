// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The script UI engine of the 4b-2 round: the script TEXT WINDOW (was
// script/textwindowengine.* + the g_TextDialog window it drove through
// OnCommandTextWindow*), the one UI-visible part of the script API.
//
// Qt shape: one singleton text dialog per app; the engine emitted
// CommandTextWindow* into the main thread and the dialog drew itself. The
// port keeps the singleton (a script that calls Open twice re-opens the same
// window - the bound is 1 window, trivially under the plan's per-script cap)
// and renders it as a small ImGui window from AssistantUI_Draw. Documented
// caveat: AssistantUI_Draw only runs while the assistant shell is visible,
// so the window vanishes with the assistant hidden (the data stays alive -
// reopening shows the same lines; Qt kept a native window alive instead).
// TextWindow* ops reach this module marshaled through the queue
// (SC_TEXT_WINDOW), so all state here is main-thread owned.
//
// SaveToFile writes the current lines (the sandbox rules of scriptfile.h
// apply; the dialog wrote the raw text with the same QFile convention).

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
    int op, const string &text, int intA, int intB, const string &path);
// The TW_SAVE body (kept public for tests).
void ScriptTextWindow_SaveToFile(const string &path);

// ImGui rendering of the window (defined in ui/assistantui.cpp next to the
// other ImGui code; AssistantUI_Draw calls it). No-op while the assistant
// shell is closed or the window was never opened.
void ScriptTextWindow_Draw();

// The current line snapshot for tests (main thread).
const vector<string> &ScriptTextWindow_Lines();
bool ScriptTextWindow_Opened();
// The window geometry for the renderer (main thread; may be null).
void ScriptTextWindow_Rect(int *x, int *y, int *width, int *height);

} // namespace xuoassist

