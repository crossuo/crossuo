// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// In-client X:UO Assist host: an ImGui overlay living in the game's own GL
// context, in-game only (ADR D9/D10/D11). Built only for

#if defined(XUO_ASSISTANT)

union SDL_Event;

// Init-or-show/hide entry point (the ToggleAssistant game macro handler);
// returns true when the toggle ran. Only works while the client is in the
// game stage.
bool AssistantWindow_Toggle();
// Consumes events targeted at assistant pop-out platform windows; while
// initialized, every event is forwarded to ImGui
// first and events it wants to capture are kept away from the game.
// Returns true when the event must not reach the game.
bool AssistantWindow_ProcessEvent(const SDL_Event &ev);
// Per-frame gating check (called from CGame::Process): wires the host bridge
// once, resets the assistant connected state as soon as the client leaves the
// in-game stage, and shuts the assistant down.
void AssistantWindow_FrameTick();
// Renders the assistant overlay at the end of the in-game render pass; the
// first call lazily initializes the assistant. Initialization may run from
// here or from the ToggleAssistant game macro: both run on the main thread
// with the game GL context current at the in-game stage.
void AssistantWindow_DrawOverlay();
// Destroys the ImGui context and every popped-out platform window; must not
// SDL_Quit as the client keeps running. Idempotent.
void AssistantWindow_Shutdown();
// Reports whether the assistant window is currently shown (consumers:
bool AssistantWindow_IsVisible();

#endif // XUO_ASSISTANT
