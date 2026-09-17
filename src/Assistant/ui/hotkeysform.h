// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The HotkeysForm window: the dockable ImGui window with the on-screen
// keyboard, the bindings table (Hotkey | Data | Enabled), the info panel
// (type/hotkey/action/run-mode), the external-code editor stub and
// Add/Delete/Copy. The engine and tables live in core/hotkeys and
// core/actions.

#pragma once

namespace xuoassist {

// Draws the window when open; *open is the window toggle (the TabHotkeys
// "Open hotkeys dialog" button flips it). Nothing draws when the assistant
// is hidden (called from AssistantUI_Draw).
void HotkeysForm_Draw(bool *open);

} // namespace xuoassist

