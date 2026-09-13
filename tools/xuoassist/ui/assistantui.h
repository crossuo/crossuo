// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef XUOASSIST_ASSISTANTUI_H
#define XUOASSIST_ASSISTANTUI_H

// Internal helper shared by AssistantWindow.cpp (lifecycle) and the ui/ tab
// implementations; not part of the client-facing API.
//
// AssistantUI_Draw keeps its Phase 0 signature/role: AssistantWindow calls it
// between ui_newframe and ui_draw while the assistant is visible. The shell
// owns the window frame, the top-level tab bar (§2.1 order), the journal pane
// and the status bar; the tab pages live in ui/tabmain.cpp, ui/tablists.cpp
// and ui/tabfilters.cpp and read/write xuoassist::g_AssistantConfig directly.

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

// Seconds left in the auto-reconnect countdown (TabMain::l_SecondsToReconnect);
// 0 when the countdown is not running.
int AssistantUI_ReconnectSecondsLeft();

// Main/Options page edit tracking: those widgets persist with the profile on
// close (Qt semantics); checklist toggles save immediately instead.
void AssistantUI_MarkOptionsDirty();
void AssistantUI_ClearOptionsDirty();

// --- tab pages (ui/tabmain.cpp, ui/tablists.cpp, ui/tabfilters.cpp) ---------
void TabMain_Draw();
// sub-tab indexes inside the Lists nested tab bar (§2.1)
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
// sub-tab indexes inside the Filters nested tab bar (§2.10)
enum TAB_FILTERS
{
    TAB_FILTERS_SPEECH = 0,
    TAB_FILTERS_REPLACES,
    TAB_FILTERS_SOUND
};
void TabFilters_Draw(int subTab);

} // namespace xuoassist

#endif // XUO_ASSISTANT
#endif // XUOASSIST_ASSISTANTUI_H
