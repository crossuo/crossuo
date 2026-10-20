// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See tabscripts.h. The tab is the script LIST: Load imports a file (native
// picker) and nothing else; double-click opens an editor; Play/Pause/Stop act
// on the selected row (also in its context menu). The list persists with the
// profile ([scripts] loadedscript0..N, assistantconfig.cpp) and the run
// identity is the file (scriptlist.h / scripthandle.h).

#include "tabscripts.h"

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include <external/gfx/imgui/imgui.h>

#include "assistantui.h" // AssistantUI_MarkOptionsDirty
#include "editors.h"
#include "scriptpicker.h"
#include "widgets.h" // DOCS_URL + OpenURL
#include "../config/assistantconfig.h"
#include "../core/assist.h"
#include "../core/assistantevents.h" // OnClientPrint
#include "../script/scriptlist.h"
#include "../script/scripthandle.h"

#include <algorithm>

namespace xuoassist {

namespace {

bool s_Restored = false;
int s_Selected = -1;
astr_t s_SelectedKey;
astr_t s_PendingRemove; // applied after the row loop

void persist_list()
{
    AssistantUI_MarkOptionsDirty();
}

void import_paths(const vector<astr_t> &paths)
{
    bool added = false;
    for (const astr_t &path : paths)
    {
        astr_t text;
        if (!ScriptList_ReadFile(path, text))
        {
            g_AssistantEvents->OnClientPrint("Cannot open: " + path);
            continue;
        }
        g_LoadedScripts.Add(path, ScriptList_ContentHash(text));
        added = true;
    }
    if (added)
    {
        persist_list();
    }
}

int row_state(const CLoadedScript &script)
{
    CScriptManager *manager = ScriptManager_Get();
    if (manager->PausedFile(script.Key))
    {
        return 2;
    }
    if (manager->RunningFile(script.Key))
    {
        return 1;
    }
    return 0;
}

void tab_play(const CLoadedScript &script)
{
    ScriptManager_PlayFile(script.Path);
}

void tab_toggle_pause(const CLoadedScript &script)
{
    ScriptManager_TogglePauseFile(script.Key);
}

void tab_stop(const CLoadedScript &script)
{
    ScriptManager_TerminateFile(script.Key);
}

void tab_open(const CLoadedScript &script)
{
    if (!Editors_Open(script.Path))
    {
        g_AssistantEvents->OnClientPrint("Cannot open: " + script.Path);
    }
}

} // namespace

void TabScripts_Draw()
{
    if (!s_Restored)
    {
        // the persisted list arrives with LoadMainOptions; the filesystem
        // checks run here (once, on the first draw)
        g_LoadedScripts.DrainPendingRestore();
        s_Restored = true;
    }

    // a Load picker result (desktop completes in place, the web page answers
    // on a later frame)
    vector<astr_t> picked;
    if (ScriptPicker_Poll(picked))
    {
        import_paths(picked);
    }

    const vector<CLoadedScript> &scripts = g_LoadedScripts.Scripts();

    // stable selection across removals/sorts
    if (!s_SelectedKey.empty())
    {
        s_Selected = -1;
        for (int i = 0; i < (int)scripts.size(); i++)
        {
            if (scripts[i].Key == s_SelectedKey)
            {
                s_Selected = i;
                break;
            }
        }
        if (s_Selected < 0)
        {
            s_SelectedKey.clear();
        }
    }
    if (s_Selected >= (int)scripts.size())
    {
        s_Selected = (int)scripts.size() - 1;
    }
    const CLoadedScript *selected =
        (s_Selected >= 0 && s_Selected < (int)scripts.size()) ? &scripts[s_Selected] : nullptr;
    const int selectedState = selected != nullptr ? row_state(*selected) : 0;

    // --- toolbar -----------------------------------------------------------
    if (ImGui::Button("Load"))
    {
        ScriptPicker_RequestLoad();
    }
    ImGui::SetItemTooltip("Imports a script file into this list (it opens in an editor on double-click)");

    ImGui::SameLine();
    ImGui::BeginDisabled(selected == nullptr || selectedState == 1);
    if (ImGui::Button("Play"))
    {
        tab_play(*selected);
    }
    ImGui::SetItemTooltip("Runs the script file (top-level). Save editor changes first: the file on disk is what runs");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(selected == nullptr || selectedState == 0);
    if (ImGui::Button(selectedState == 2 ? "Resume" : "Pause"))
    {
        tab_toggle_pause(*selected);
    }
    ImGui::SetItemTooltip(selectedState == 2 ? "Resumes the paused script" : "Pauses the running script");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(selected == nullptr || selectedState == 0);
    if (ImGui::Button("Stop"))
    {
        tab_stop(*selected);
    }
    ImGui::SetItemTooltip("Terminates the running script");
    ImGui::EndDisabled();

    // --- options -----------------------------------------------------------
    bool sortScripts = g_AssistantConfig.SortScripts();
    if (ImGui::Checkbox("Sort scripts in lists", &sortScripts))
    {
        g_AssistantConfig.SetSortScripts(sortScripts);
        persist_list();
    }
    ImGui::Separator();

    // --- the loaded-scripts list -------------------------------------------
    // display order: list order, or display-name order with the sort option
    vector<int> order;
    order.reserve(scripts.size());
    for (int i = 0; i < (int)scripts.size(); i++)
    {
        order.push_back(i);
    }
    if (sortScripts)
    {
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
            return str_lower(scripts[a].Path) < str_lower(scripts[b].Path);
        });
    }

    const float footerHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0f;
    if (ImGui::BeginListBox("##loadedscripts", ImVec2(-1, -footerHeight)))
    {
        if (order.empty())
        {
            ImGui::TextDisabled("No scripts loaded. Press Load to add one.");
        }
        for (int index : order)
        {
            const CLoadedScript &script = scripts[index];
            ImGui::PushID(script.Key.c_str());

            const int state = row_state(script);
            const char *status = state == 2 ? "[paused]" : state == 1 ? "[running]" : "";
            char label[512];
            snprintf(label, sizeof(label), "%s%s%s", ScriptList_DisplayName(script.Path).c_str(),
                status[0] != 0 ? " " : "", status);
            if (!script.bExists)
            {
                snprintf(label, sizeof(label), "%s [missing]",
                    ScriptList_DisplayName(script.Path).c_str());
            }

            const bool isSelected = index == s_Selected;
            if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                s_Selected = index;
                s_SelectedKey = script.Key;
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    tab_open(script);
                }
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", script.Path.c_str());
            }

            if (ImGui::BeginPopupContextItem())
            {
                s_Selected = index;
                s_SelectedKey = script.Key;
                if (ImGui::MenuItem("Open in editor"))
                {
                    tab_open(script);
                }
                ImGui::Separator();
                ImGui::BeginDisabled(state == 1);
                if (ImGui::MenuItem("Play"))
                {
                    tab_play(script);
                }
                ImGui::EndDisabled();
                ImGui::BeginDisabled(state == 0);
                if (ImGui::MenuItem(state == 2 ? "Resume" : "Pause"))
                {
                    tab_toggle_pause(script);
                }
                if (ImGui::MenuItem("Stop"))
                {
                    tab_stop(script);
                }
                ImGui::EndDisabled();
                ImGui::Separator();
                if (ImGui::MenuItem("Reload from disk", nullptr, false, Editors_Find(script.Key) != nullptr))
                {
                    Editors_ReloadFromDisk(script.Path);
                }
                if (ImGui::MenuItem("Remove from list"))
                {
                    s_PendingRemove = script.Key;
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }

    if (!s_PendingRemove.empty())
    {
        // removing a loaded script stops its instance first (a list entry
        // the user cannot see must not keep running), then drops the entry
        ScriptManager_TerminateFile(s_PendingRemove);
        g_LoadedScripts.Remove(s_PendingRemove);
        if (s_SelectedKey == s_PendingRemove)
        {
            s_SelectedKey.clear();
        }
        s_PendingRemove.clear();
        persist_list();
    }

    // --- footer ------------------------------------------------------------
    ImGui::TextDisabled("%d script(s) | double-click a script to edit it", (int)scripts.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Script examples"))
    {
        OpenURL(DOCS_URL);
    }

    // keep the handles list clean (finished scripts leave the list)
    ScriptManager_Get()->Tick();
}

} // namespace xuoassist
