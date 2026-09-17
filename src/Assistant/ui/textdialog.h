// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The Text Dialog window: one full-area multi-line text surface.
// The script-driven half (TextWindow.Open/Clear/Print/Save + the journal/menu
// dumps) lives in script/scriptgump.cpp and draws as the "Script text window".
// This window is the assistant's text-response composition surface - one
// multi-line input with Send/Cancel wired to the prompt response flow
// (g_PromptManager.Send is the same entry the packet path and the scripts
// use). A server prompt that reaches the window while it is open flips the
// status line; the window never auto-opens (the client owns the prompt bar
// for the default flow).

#pragma once

#if defined(XUO_ASSISTANT)

namespace xuoassist {

// The window body (assistantui.cpp draws it while the assistant is visible).
void TextDialog_Draw();

// Shows the window; there is exactly one instance.
void TextDialog_Open();

// Shutdown (context alive): clears the input buffer state.
void TextDialog_Shutdown();

} // namespace xuoassist

#endif // XUO_ASSISTANT
