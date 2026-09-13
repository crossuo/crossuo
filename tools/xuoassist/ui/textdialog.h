// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The Text Dialog window (was assist/textdialog.ui/.cpp, 457x473, one
// full-area plain-text-edit).
//
// Port note: the original dialog's script-driven half (TextWindow.Open/Clear/
// Print/Save + the journal/menu dumps) already lives in script/scriptgump.cpp
// and draws as the "Script text window". What stayed unported was the free
// text surface itself: this window mirrors the dialog's remaining role as the
// assistant's text-response composition surface - one multi-line input with
// Send/Cancel wired to the prompt response flow (g_PromptManager.Send is the
// same entry the packet path and the scripts use). A server prompt that
// reaches the window while it is open flips the status line; the window never
// auto-opens (the client owns the prompt bar for the default flow, and the original
// dialog was always explicitly opened by the API/user too).

#pragma once

#if defined(XUO_ASSISTANT)

namespace xuoassist {

// The window body (assistantui.cpp draws it while the assistant is visible).
void TextDialog_Draw();

// Shows the window (entry point for later user surface; the window keeps the
// single-instance semantics of the original dialog - there is exactly one).
void TextDialog_Open();

// Shutdown (context alive): clears the input buffer state.
void TextDialog_Shutdown();

} // namespace xuoassist

#endif // XUO_ASSISTANT
