// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The script editor window: toolbar New/Open/Save/SaveAs/Find/GoToLine/
// Replace/Comment-selection, the TextEditor widget, the statusbar, and the
// Find / Replace / Go to line popups. A simple floating ImGui window; the
// map window shows the shared dockable/pop-out pattern.

#include "../core/global.h"

namespace xuoassist {

// Draws the editor window; `open` is the visibility flag: the X button
// flips it and the window skips drawing while it is false (reopening
// through the flag - the tab button / script start - resets it). Saves
// through the tab when the path is set (Save falls back to Autoload.oajs
// like TabScripts::SaveScript).
void ScriptEditorWindow_Draw(bool *open);

} // namespace xuoassist

