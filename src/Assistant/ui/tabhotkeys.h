// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// TabHotkeys: the On/Off + "Pass keys to client" switches, the command prefix
// field, the Load/Save buttons, the hotkeys-dialog launcher and the three
// speech-only control launchers (Animal/Vendor, House, Ship).
//
// "Pass keys to client" semantics: a matched hotkey is consumed by the
// assistant unless the checkbox is set - then the key ALSO reaches the game
// (core/hotkeys.cpp MatchAndUse). While any assistant text field has keyboard
// focus (io.WantTextInput) the game never sees keys - AssistantWindow.cpp
// gates that ahead of the engine.

#pragma once

namespace xuoassist {

void TabHotkeys_Draw();
// The pop-out control windows (drawn while the assistant is visible; each has
// its own title bar and can be docked).
void TabHotkeys_DrawControlForms();

// Drops the control-form from-target arm state (CancelPendingFromTargets,
// shutdown: a request armed before shutdown must not swallow a 0x6C after
// re-init).
void TabHotkeys_CancelPendingFromTargets();

} // namespace xuoassist

