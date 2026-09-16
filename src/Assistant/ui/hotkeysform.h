// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The HotkeysForm window (Phase 3, master plan §3.1/§7 item 1): the 851x639
// dockable ImGui window with the on-screen keyboard, the bindings table
// (Hotkey | Data | Enabled), the info panel (type/hotkey/action/run-mode),
// the external-code editor stub and Add/Delete/Copy. Was
// assist/hotkeysform.{ui,cpp}; the engine and tables live in core/hotkeys
// and core/actions.
//
// Persistence: every mutation saves Hotkeys.ini immediately (the original app
// flagged m_WantToSave and saved from a 300 ms timer).

#pragma once

namespace xuoassist {

// Draws the window when open; *open is the window toggle (the TabHotkeys
// "Open hotkeys dialog" button flips it). Nothing draws when the assistant
// is hidden (called from AssistantUI_Draw).
void HotkeysForm_Draw(bool *open);

} // namespace xuoassist

