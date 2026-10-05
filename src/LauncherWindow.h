// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef _LAUNCHER_WINDOW_H_
#define _LAUNCHER_WINDOW_H_

// Embedded X:UO Launcher host (GL3 builds only; other renderer builds run
// without a launcher). See CMake XUO_LAUNCHER_EMBEDDED.

#if defined(XUO_LAUNCHER_EMBEDDED)

union SDL_Event;

// Creates the launcher window, GL context and the launcher app.
bool LauncherWindow_Init();
// Destroys the launcher window; must not SDL_Quit as the client keeps running.
void LauncherWindow_Shutdown();
// Show or hide the launcher window.
void LauncherWindow_Show(bool show);
// Opens the launcher, creating it on demand (login screen button).
void LauncherWindow_Open();
bool LauncherWindow_IsVisible();
// Consumes events targeted at the launcher window; returns true when handled.
// Also implements the global hotkey (Alt+L) to toggle the window.
bool LauncherWindow_ProcessEvent(const SDL_Event &ev);
// Renders the launcher UI into its own window, when visible.
void LauncherWindow_Render();
// Boot-mode pump: drives the launcher by itself while the client waits for a
// valid configuration. Returns false when the user closed it.
bool LauncherWindow_Pump();

#endif // XUO_LAUNCHER_EMBEDDED
#endif // _LAUNCHER_WINDOW_H_
