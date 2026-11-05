// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// In-client Asset Viewer: a read-only ImGui browser for the UO data files the
// client already has mapped (MUL and UOP). The module shares the assistant's
// ImGui context and frame pipeline (it is drawn inside the assistant overlay
// pass), so it is built only in the same configurations as the assistant.

#if defined(XUO_ASSISTANT)

// Init-or-show/hide entry point; returns true when the toggle ran.
bool AssetViewer_Toggle();
// Shows or hides the asset viewer window; returns true when the request ran.
bool AssetViewer_Show(bool show);
// Reports whether the asset viewer window is currently shown.
bool AssetViewer_IsVisible();
// Reports whether the viewer window is shown (not closed by its X / the
// client window's X). The viewer keeps running while closed: it owns the
// cursor and input over its area, and the assistant's overlay keeps coming
// back for it even when the assistant shell itself is shut down.
bool AssetViewer_IsWindowOpen();
// Whole-assistant shutdown: when the viewer window is open it keeps living on
// the ImGui context (the assistant keeps the context alive just for it), so
// there is nothing to give back here; kept as the single hook for that life
// cycle.
void AssetViewer_OnAssistantShutdown();
// Ends the viewer session (the assistant "Assets" button starts a new one):
// hides the window, stops the sounds/music the viewer itself started and
// leaves the client's own music untouched. Returns true when a session was
// active.
bool AssetViewer_Close();
// Submits the asset viewer window inside the assistant's ImGui frame (called
// from AssistantWindow_DrawOverlay, after the assistant shell, so the window
// participates in the same docking/viewport pass). A no-op while hidden.
void AssetViewer_Draw();
// Releases every cached texture's CPU pixels; called after the assistant (and
// with it the ImGui context) is shut down. Idempotent.
void AssetViewer_Shutdown();

#endif // XUO_ASSISTANT
