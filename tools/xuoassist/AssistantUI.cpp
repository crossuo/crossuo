// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Assistant

#include "AssistantUI.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>
#include "GameObjects/GameWorld.h" // g_World
#include "Globals.h"               // g_Ticks

void AssistantUI_Draw(bool *open, uint32_t sessionStartTicks)
{
    if (!ImGui::Begin("X:UO Assist", open))
    {
        // collapsed: the window X button already flipped *open for us
        ImGui::End();
        return;
    }

    // header row: pop out of / dock back into the game window (ADR D11)
    const ImGuiViewport *mainViewport = ImGui::GetMainViewport();
    const bool poppedOut = ImGui::GetWindowViewport() != mainViewport;
    if (poppedOut)
    {
        if (ImGui::Button("Dock in game window"))
        {
            ImGui::SetWindowPos(ImVec2(
                mainViewport->Pos.x + 32.0f,
                mainViewport->Pos.y + 32.0f));
        }
    }
    else
    {
        // placing the window outside the main viewport work area makes ImGui
        // spawn a real OS window for it on the next frame
        if (ImGui::Button("Pop out"))
        {
            ImGui::SetWindowPos(ImVec2(
                mainViewport->Pos.x + mainViewport->WorkSize.x + 48.0f,
                mainViewport->Pos.y + 48.0f));
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("X:UO Assist - Phase 0 skeleton");
    ImGui::TextUnformatted("Alt+U shows/hides the assistant.");
    ImGui::Separator();

    // status bar (mirrors the Qt assistant information line)
    const uint32_t elapsed = g_Ticks - sessionStartTicks;
    const int hours = int(elapsed / (60u * 60u * 1000u));
    const int minutes = int((elapsed / (60u * 1000u)) % 60u);
    // trivial world census: every object the client currently tracks
    const int items = g_World != nullptr ? int(g_World->m_Map.size()) : 0;
    ImGui::TextDisabled(
        "Running time: %dh %dm | Items: %d. Developed by CrossUO Team.",
        hours,
        minutes,
        items);

    ImGui::End();
}

#endif // XUO_ASSISTANT
