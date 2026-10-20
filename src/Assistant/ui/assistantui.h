// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// Internal helper shared by AssistantWindow.cpp (lifecycle) and the ui/ tab
// implementations; not part of the client-facing API.
//
// AssistantUI_Draw: AssistantWindow calls it between ui_newframe and ui_draw
// while the assistant is visible. The shell owns the window frame, the
// top-level tab bar, the journal pane and the status bar; the tab pages live
// in ui/tabmain.cpp, ui/tablists.cpp and ui/tabfilters.cpp and read/write
// xuoassist::g_AssistantConfig directly.

#include <stdint.h>

#if defined(XUO_ASSISTANT)

namespace xuoassist {

// Draws the assistant main window (dockable, pop-out aware). open is the
// visibility flag owned by AssistantWindow.cpp (the window X button hides the
// assistant through it); sessionStartTicks is the g_Ticks value captured when
// the assistant was initialized.
void AssistantUI_Draw(bool *open, uint32_t sessionStartTicks);

// Lifecycle extensions called by AssistantWindow.cpp: install the UI events
// consumer (journal pane feed), run the per-frame logic that must continue
// while the window is hidden (auto-reconnect countdown), and flush pending
// option edits + uninstall the events consumer on shutdown.
void AssistantUI_Init();
void AssistantUI_Tick();
void AssistantUI_Shutdown();

// The deferred Autostart run: the assistant shell ticks it while visible.
void AssistantUI_QueueAutostart();

// The closeuo request: pushes the client's own quit event through the
// windowing layer.
void AssistantUI_RequestClientClose();

// The UI-layer from-target disarm: re-arms when the assistant UI re-inits.
void CancelPendingFromTargets();

// Seconds left in the auto-reconnect countdown (TabMain::l_SecondsToReconnect);
// 0 when the countdown is not running.
int AssistantUI_ReconnectSecondsLeft();

// Main/Options page edit tracking: those widgets persist with the profile on
// close (original semantics); checklist toggles save immediately instead.
void AssistantUI_MarkOptionsDirty();
void AssistantUI_ClearOptionsDirty();

// Recompiles the display title data (g_DisplayManager) from the config - was
// AssistForm::LoadCurrentProfile's g_TabDisplay->RecompileData() tail;
// called at init and after a profile switch.
void AssistantUI_RecompileDisplayTitle();

// The post-shutdown half of the texture lifecycle (runs from
// AssistantWindow.cpp right AFTER ui_shutdown/DestroyContext): frees the CPU
// pixels of the UI-owned textures (icon atlas, map radar, About logo) whose
// GL names the backend teardown reclaimed while the context was alive.
void AssistantUI_AfterImGuiShutdown();


// --- secondary windows ----------------------------------------------------------
// The Map window + its settings form (ui/mapwindow.cpp), the Object Inspector
// (ui/inspector.cpp) and the Text Dialog (ui/textdialog.cpp): dockable/poppable
// ImGui windows drawn by the shell while the assistant is visible, gated by
// their own open flags (MapWindow_Open / Inspector_Open / TextDialog_Open).
void TabAbout_Draw();
// The About logo CPU-pixels release (the atlas ordering rule): runs
void TabAbout_AfterImGuiShutdown();

// --- tab pages (ui/tabmain.cpp, ui/tablists.cpp, ui/tabfilters.cpp) ---------
void TabMain_Draw();
// sub-tab indexes inside the Lists nested tab bar
enum TAB_LISTS
{
    TAB_LISTS_TYPES = 0,
    TAB_LISTS_OBJECTS,
    TAB_LISTS_FIND,
    TAB_LISTS_IGNORE,
    TAB_LISTS_FRIENDS,
    TAB_LISTS_ENEMIES
};
void TabLists_Draw(int subTab);

// Drops every list-tab from-target arm state (CancelPendingFromTargets,
// shutdown: a request armed before shutdown must not swallow a 0x6C after
// re-init).
void TabLists_CancelPendingFromTargets();
// sub-tab indexes inside the Filters nested tab bar
enum TAB_FILTERS
{
    TAB_FILTERS_SPEECH = 0,
    TAB_FILTERS_REPLACES,
    TAB_FILTERS_SOUND
};
void TabFilters_Draw(int subTab);

// --- phase 4a tab pages ------------------------------------------------------
// Agents -> the Dress/Party/Shop nested bar; the shop tab owns
void TabDress_Draw();
void TabParty_Draw();
void TabShop_Draw();
void TabSkills_Draw();
void TabDisplay_Draw();

// Closes the shop list editor (shutdown: drop the working copy silently).
void TabShop_CloseEditor();
// Draws the shopping progress popup whenever the shop agent is active,
// regardless of the visible tab (no-op otherwise).
void TabShop_DrawProgress();
// Marks the display caption for recompose+apply (the OnTitleRedraw events
// and the Display tab edits land here).
void TabDisplay_RequestTitleRedraw();
// Returns and clears the title-redraw flag (AssistantUI_Tick).
bool TabDisplay_ConsumeTitleDirty();

// Per-tab from-target disarms (CancelPendingFromTargets,
// shutdown: a request armed before shutdown must not swallow a 0x6C after
// re-init).
void TabDress_CancelPendingFromTargets();
void TabParty_CancelPendingFromTargets();

} // namespace xuoassist

#endif // XUO_ASSISTANT
