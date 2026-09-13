// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See tabscripts.h. Config persistence keeps the original element (tabscripts.cpp
// LoadConfig/SaveConfig: <scripts sortscripts=".." scriptfile="..">), which
// the Phase 1c config port already reads/writes into
// g_AssistantConfig.SortScripts()/ScriptFilePath().

#include "tabscripts.h"

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include <external/gfx/imgui/imgui.h>

#include "assistantui.h" // g_AssistantScriptEditorOpen
#include "../config/assistantconfig.h"
#include "../core/assist.h"
#include "scripteditor.h"
#include "scriptmenu.h"
#include "widgets.h" // DOCS_URL + OpenURL
#include "../script/scripthandle.h"

#include <algorithm>

namespace xuoassist {

namespace {

char s_ScriptPath[512] = { 0 };
bool s_PathLoaded = false;
int s_SelectedFunction = 0;
char s_FunctionNameInput[128] = { 0 };

void sync_path_from_config()
{
    if (s_PathLoaded)
    {
        return;
    }
    snprintf(s_ScriptPath, sizeof(s_ScriptPath), "%s", g_AssistantConfig.ScriptFilePath().c_str());
    s_PathLoaded = true;
    if (s_ScriptPath[0] != 0)
    {
        // the original Init() auto-loaded the saved script file
        ScriptManager_LoadScriptFile(s_ScriptPath);
    }
}

void load_script_path(const char *path)
{
    // callers may pass s_ScriptPath itself (the ImGui edit buffer), and
    // snprintf forbids overlap between destination and source (-Wrestrict):
    // take a local copy of the input first
    char pathCopy[512];
    snprintf(pathCopy, sizeof(pathCopy), "%s", path);
    snprintf(s_ScriptPath, sizeof(s_ScriptPath), "%s", pathCopy);
    g_AssistantConfig.SetScriptFilePath(pathCopy);
    // the path persists with the profile (the original tab saved on close)
    AssistantUI_MarkOptionsDirty();
    ScriptManager_LoadScriptFile(pathCopy);
}

} // namespace

void TabScripts_Draw()
{
    sync_path_from_config();

    // --- the file path row --------------------------------------------------
    ImGui::SetNextItemWidth(-140);
    if (ImGui::InputText("##scriptpath", s_ScriptPath, sizeof(s_ScriptPath), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        load_script_path(s_ScriptPath);
    }
    ImGui::SameLine();
    // the browse tool-button reuses the editor window's picker: the cheapest
    // faithful path is to hand the path to the editor and let it load
    if (ImGui::Button("...##browse"))
    {
        // handled in the editor window this round (shared picker); open it
        g_Assistant.ClientPrint("Browse: use the editor window's Open dialog (shared file picker).");
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload##script"))
    {
        load_script_path(s_ScriptPath); // reload = stop-free re-read + re-parse
    }
    ImGui::SetItemTooltip("Reload current script file");

    // --- running actions row --------------------------------------------------
    // The function combo honors the SortScripts option (the original
    // UpdateScriptsList tail sorted ONLY cb_ScriptNameForStart -
    // the running list stayed in start order).
    const vector<string> functions = ScriptEditor_FunctionList();
    vector<std::pair<string, int>> combo; // display name -> functions[] index
    combo.reserve(functions.size());
    for (int i = 0; i < (int)functions.size(); i++)
    {
        combo.emplace_back(functions[i], i);
    }
    if (g_AssistantConfig.SortScripts())
    {
        std::sort(combo.begin(), combo.end(),
            [](const std::pair<string, int> &a, const std::pair<string, int> &b) {
                return a.first < b.first;
            });
    }
    if (s_SelectedFunction >= (int)combo.size())
    {
        s_SelectedFunction = 0;
    }
    const string selectedName =
        combo.empty() ? string() : combo[s_SelectedFunction].first;
    ImGui::SetNextItemWidth(240);
    if (!combo.empty())
    {
        if (ImGui::BeginCombo("##runfunction", combo[s_SelectedFunction].first.c_str()))
        {
            for (int i = 0; i < (int)combo.size(); i++)
            {
                const bool selected = i == s_SelectedFunction;
                if (ImGui::Selectable(combo[i].first.c_str(), selected))
                {
                    s_SelectedFunction = i;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }
    else
    {
        // no parsed functions: keep the editable name input
        ImGui::SetNextItemWidth(240);
        ImGui::InputText("##runname", s_FunctionNameInput, sizeof(s_FunctionNameInput));
    }
    ImGui::SameLine();
    if (ImGui::Button("Start"))
    {
        const string name = combo.empty() ? string(s_FunctionNameInput) : selectedName;
        if (!name.empty())
        {
            ScriptManager_Get()->Run(name, ScriptEditor_Text());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Pause / resume"))
    {
        for (CScriptHandle *handle : ScriptManager_Get()->Handles())
        {
            if (handle->Name() == (combo.empty() ? string(s_FunctionNameInput) : selectedName))
            {
                handle->SetPaused(!handle->GetPaused());
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        ScriptManager_Terminate(
            combo.empty() ? string(s_FunctionNameInput) : selectedName, "");
    }

    // --- running scripts list --------------------------------------------------
    ImGui::Separator();
    ImGui::TextUnformatted("Running scripts list:");
    if (ImGui::BeginListBox("##runningscripts", ImVec2(-1, 120)))
    {
        for (CScriptHandle *handle : ScriptManager_Get()->Handles())
        {
            ImGui::PushID(handle);
            const char *stateIcon = handle->State() == SS_PAUSED ? "[paused]" :
                handle->State() == SS_FAILED ? "[failed]" :
                handle->State() == SS_ZOMBIE ? "[zombie]" : "[running]";
            ImGui::Selectable("", false);
            ImGui::SameLine();
            ImGui::Text("%s %s", stateIcon, handle->Name().c_str());
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }

    // --- options row -----------------------------------------------------------
    bool sortScripts = g_AssistantConfig.SortScripts();
    if (ImGui::Checkbox("Sort scripts in lists", &sortScripts))
    {
        // the checkbox re-sorts the function combo (UpdateScriptsList tail)
        g_AssistantConfig.SetSortScripts(sortScripts);
        AssistantUI_MarkOptionsDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button("Open editor"))
    {
        // was pb_ScriptOpenEditor -> g_ScriptEditorDialog->show()
        g_AssistantScriptEditorOpen = true;
    }
    ImGui::SetItemTooltip("Opens the script editor window with the loaded file");

    // clb_ScriptExamples: the original command link opened the ScriptLibrary URL;
    // now the docs page (hosts the examples), same launcher as the About tab
    if (ImGui::SmallButton("Script examples"))
    {
        OpenURL(DOCS_URL);
    }
    ImGui::SetItemTooltip("Opens the docs page (hosts the examples collection)");

    // keep the handles list clean (finished scripts leave the list)
    ScriptManager_Get()->Tick();
}

} // namespace xuoassist
