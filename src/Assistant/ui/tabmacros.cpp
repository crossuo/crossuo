// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// TabMacros implementation (see tabmacros.h). The original button handlers map 1:1;
// the actions-list text cache exists so the draw path builds no strings (the
// original rows were CMacroActionListItem objects).

#define LOGGER_MODULE Assistant

#include "tabmacros.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include <stdio.h>

#include "widgets.h"
#include "../config/assistantconfig.h"
#include "../core/assist.h"
#include "../core/global.h"
#include "../macros/macro.h"
#include "../macros/macroexecutor.h"

namespace xuoassist {

namespace {

int s_SelectedMacro = -1;

static const char *MacroNameOf(const CMacroEntry &entry) { return entry.Name.c_str(); }
static void MacroSetNameOf(CMacroEntry &entry, const char *name) { entry.Name = name; }

// the actions-list text cache (rebuilt on selection/commit, not per frame)
vector<astr_t> s_ActionTexts;
int s_ActionTextsMacro = -2;
size_t s_ActionTextsCount = 0;

void invalidate_action_texts()
{
    s_ActionTextsMacro = -2;
}

void rebuild_action_texts()
{
    s_ActionTexts.clear();
    const vector<CMacroEntry> &macros = g_AssistantConfig.Macros();
    if (s_SelectedMacro >= 0 && s_SelectedMacro < (int)macros.size())
    {
        for (CMacro *action : macros[s_SelectedMacro].m_Items)
        {
            s_ActionTexts.push_back(action->GetText());
        }
        s_ActionTextsCount = macros[s_SelectedMacro].m_Items.size();
    }
    else
    {
        s_ActionTextsCount = 0;
    }
    s_ActionTextsMacro = s_SelectedMacro;
}

// CExtendedListWidget::GetFreeName(nullptr, "macro") for the New button
astr_t free_macro_name()
{
    const vector<CMacroEntry> &macros = g_AssistantConfig.Macros();
    astr_t name = "macro";
    int offset = 1;
    while (true)
    {
        bool free = true;
        for (int i = (int)macros.size() - 1; i >= 0; i--)
        {
            if (ListNamesEqual(macros[i].Name, name.c_str()))
            {
                free = false;
                break;
            }
        }
        if (free)
        {
            return name;
        }
        name = "macro_" + str_from(offset++);
    }
}

void save_macros()
{
    g_AssistantConfig.SaveMacros();
    invalidate_action_texts();
}

} // namespace

void TabMacros_Draw()
{
    vector<CMacroEntry> &macros = g_AssistantConfig.m_Macros;
    const bool recording = g_MacroExecutor.IsRecording();
    const bool playingThis =
        g_MacroExecutor.IsPlaying() && g_MacroExecutor.PlayingIndex() == s_SelectedMacro;

    // the macro list pane left, the actions pane right
    ImGui::BeginChild("##macros_pane", ImVec2(100.0f, 0.0f), ImGuiChildFlags_None);
    ImGui::TextUnformatted("Macro list:");
    const bool listChanged = NameList(
        "##macro_list", macros, &s_SelectedMacro, &MacroNameOf, &MacroSetNameOf, false, 150.0f);
    if (listChanged)
    {
        // renames/reorders/deletes through the list core: renamed macros
        // re-point their hotkeys by name at the next save (UpdateMacroList)
        save_macros();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##macros_form", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);

    // New / Delete (on_pb_Create_clicked / on_pb_Remove_clicked)
    ImGui::BeginDisabled(recording || playingThis);
    if (ImGui::Button("New"))
    {
        CMacroEntry entry;
        entry.Name = free_macro_name();
        macros.push_back(entry);
        s_SelectedMacro = (int)macros.size() - 1;
        invalidate_action_texts();
        save_macros();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete"))
    {
        if (s_SelectedMacro >= 0 && s_SelectedMacro < (int)macros.size())
        {
            macros.erase(macros.begin() + s_SelectedMacro);
            s_SelectedMacro = -1;
            invalidate_action_texts();
            save_macros();
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();

    // --- Record / Play -------------------------------------------------------
    if (recording)
    {
        if (ImGui::Button("Stop"))
        {
            // on_pb_Record_clicked stop half: commit or discard
            if (g_MacroExecutor.RecordStop() > 0)
            {
                save_macros();
            }
        }
        ImGui::SameLine();
        ImGui::TextColored(
            ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "recording: target clicks are captured");
    }
    else if (ImGui::Button("Record"))
    {
        // on_pb_Record_clicked start half
        if (!g_MacroExecutor.IsPlaying() && s_SelectedMacro >= 0 &&
            s_SelectedMacro < (int)macros.size())
        {
            g_MacroExecutor.RecordStart(s_SelectedMacro);
        }
    }
    ImGui::SameLine();

    if (playingThis)
    {
        if (ImGui::Button("Stop"))
        {
            g_MacroExecutor.Stop();
        }
    }
    else
    {
        ImGui::BeginDisabled(recording);
        if (ImGui::Button("Play"))
        {
            // on_pb_Play_clicked -> Play(macroItem, false)
            if (s_SelectedMacro >= 0 && s_SelectedMacro < (int)macros.size())
            {
                g_MacroExecutor.PlayIndex(s_SelectedMacro, false);
            }
        }
        ImGui::EndDisabled();
    }
    ImGui::SameLine();

    // pb_ToScript stays disabled (the script engine is script-module only)
    ImGui::BeginDisabled(true);
    ImGui::Button("To script");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        ImGui::SetTooltip("Not available.");
    }
    ImGui::EndDisabled();
    ImGui::SameLine();

    // --- loop controls (cb_Looped + dsb_LoopTime, 0.1-1000) -----------------
    static bool looped = false;
    static float loopSeconds = 1.0f;
    ImGui::Checkbox("Loop, sec:", &looped);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::InputFloat("##loop_seconds", &loopSeconds, 0.0f, 0.0f, "%.1f");
    if (loopSeconds < 0.1f)
    {
        loopSeconds = 0.1f;
    }
    if (loopSeconds > 1000.0f)
    {
        loopSeconds = 1000.0f;
    }

    // --- the actions list (lw_MacroActions) ---------------------------------
    ImGui::TextUnformatted("Macro actions list:");
    if (s_SelectedMacro != s_ActionTextsMacro ||
        (s_SelectedMacro >= 0 && s_SelectedMacro < (int)macros.size() &&
         macros[s_SelectedMacro].m_Items.size() != s_ActionTextsCount))
    {
        rebuild_action_texts();
    }
    if (ImGui::BeginListBox("##macro_actions", ImVec2(-1.0f, 150.0f)))
    {
        for (const astr_t &text : s_ActionTexts)
        {
            ImGui::Selectable(text.c_str(), false, ImGuiSelectableFlags_Disabled);
        }
        ImGui::EndListBox();
    }

    ImGui::EndChild();

    // push the loop options
    g_MacroExecutor.SetLoopOptions(looped && EnabledFeatureLoopedMacro(), loopSeconds);
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
