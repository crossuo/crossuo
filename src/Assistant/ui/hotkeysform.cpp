// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The HotkeysForm window implementation (see hotkeysform.h). Ported from
// assist/hotkeysform.cpp with the ImGui widget kit; behaviors referenced by
// their original line numbers.

#define LOGGER_MODULE Assistant

#include "hotkeysform.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include <stdio.h>
#include <string.h>

#include "widgets.h"
#include "../config/assistantconfig.h"
#include "../core/actions.h"
#include "../core/hotkeys.h"

namespace xuoassist {

namespace {

// --- static UI state (frame-static: the draw path allocates nothing) -------

bool s_FormOpen = false;
int s_SelectedRow = -1;      // the bindings table selection
uint32_t s_SelectedType = 0;     // the info panel "Type" combo
int s_SelectedAction = 0;    // the info panel "Action" combo
char s_HotkeyText[96] = { 0 };  // the capture field label
char s_ExternalCode[2048] = { 0 }; // the "External code" editor (plain text)
char s_DataText[512] = { 0 };   // Data column scratch (one row per frame)

constexpr float FORM_WIDTH = 851.0f;  // the original default form size
constexpr float FORM_HEIGHT = 639.0f;

void refresh_hotkey_text()
{
    if (s_SelectedRow >= 0 && s_SelectedRow < (int)g_AssistantConfig.m_Hotkeys.size())
    {
        const CHotkeyEntry &entry = g_AssistantConfig.m_Hotkeys[s_SelectedRow];
        // GetHotkeyText(0, ..) renders the "No Key" placeholder
        HotkeyFormatText(entry.Key, entry.Modifiers, s_HotkeyText, sizeof(s_HotkeyText));
    }
    else
    {
        snprintf(s_HotkeyText, sizeof(s_HotkeyText), "Click to capture");
    }
}

void select_row(int row)
{
    // on_tw_Hotkeys_clicked: load the row into the
    // info panel (m_Loading-guarded)
    s_SelectedRow = row;
    if (row < 0 || row >= (int)g_AssistantConfig.m_Hotkeys.size())
    {
        s_SelectedRow = -1;
        return;
    }
    const CHotkeyEntry &entry = g_AssistantConfig.m_Hotkeys[row];

    s_SelectedType = entry.Type;
    s_SelectedAction = (int)entry.Action;
    if (entry.Type == HT_PLAY_MACRO || entry.Type == HT_SCRIPT)
    {
        // match the stored name against the action list
        s_SelectedAction = -1;
        char name[128];
        const int count = Actions_ActionCount(entry.Type);
        for (int i = count - 1; i >= 0; i--)
        {
            Actions_ActionName(entry.Type, i, name, sizeof(name));
            if (name == entry.ScriptOrMacroName)
            {
                s_SelectedAction = i;
                break;
            }
        }
        if (s_SelectedAction < 0)
        {
            s_SelectedAction = 0;
        }
    }
    snprintf(s_ExternalCode, sizeof(s_ExternalCode), "%s", entry.ExternalCode.c_str());
    refresh_hotkey_text();
}

void commit_action_selection()
{
    // on_cb_Action_currentIndexChanged
    if (s_SelectedRow < 0 || s_SelectedRow >= (int)g_AssistantConfig.m_Hotkeys.size())
    {
        return;
    }
    CHotkeyEntry &entry = g_AssistantConfig.m_Hotkeys[s_SelectedRow];
    if (entry.Type == HT_PLAY_MACRO || entry.Type == HT_SCRIPT)
    {
        char name[128] = { 0 };
        Actions_ActionName(entry.Type, s_SelectedAction, name, sizeof(name));
        if (name[0] != '\0')
        {
            entry.ScriptOrMacroName = name;
        }
    }
    else
    {
        entry.Action = (uint32_t)s_SelectedAction;
    }
    g_AssistantConfig.SaveHotkeys();
}

} // namespace

void HotkeysForm_KeyboardAssign(int keyId)
{
    g_Hotkeys.KeyboardAssign(keyId, s_SelectedRow);
    refresh_hotkey_text();
}

void HotkeysForm_Draw(bool *open)
{
    if (open != nullptr)
    {
        s_FormOpen = *open;
    }
    if (!s_FormOpen)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(FORM_WIDTH, FORM_HEIGHT), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Hotkeys", &s_FormOpen))
    {
        ImGui::End();
        if (open != nullptr)
        {
            *open = false;
        }
        return;
    }

    // --- the duplicate question popup (CheckExists) ---
    if (g_Hotkeys.DuplicateRow() != -1)
    {
        ImGui::OpenPopup("Hotkey exists");
    }
    if (ImGui::BeginPopupModal("Hotkey exists", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("A hotkey with this combination already exists.");
        ImGui::TextUnformatted("What do you want to do?");
        if (ImGui::Button("Replace"))
        {
            g_Hotkeys.ResolveDuplicate(0);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Duplicate"))
        {
            g_Hotkeys.ResolveDuplicate(1);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Select Existing"))
        {
            const int dup = g_Hotkeys.DuplicateRow();
            g_Hotkeys.ResolveDuplicate(2);
            select_row(dup); // the original jumped to the existing row
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // --- left: keyboard + bindings table | right: info panel --------------
    const float infoWidth = 300.0f;

    ImGui::BeginChild("##keyboard_area", ImVec2(ImGui::GetContentRegionAvail().x - infoWidth, 0.0f));

    HotkeyKeyboard_Draw("##hotkey_keyboard", s_SelectedRow);

    ImGui::Separator();

    // the bindings table (tw_Hotkeys: "Hotkey" | "Data", col0 checkbox)
    static ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
                                   ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("##hotkeys_table", 3, flags, ImGui::GetContentRegionAvail()))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 30.0f);
        ImGui::TableSetupColumn("Hotkey", ImGuiTableColumnFlags_WidthFixed, 197.0f);
        ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        const vector<CHotkeyEntry> &hotkeys = g_AssistantConfig.m_Hotkeys;
        for (int i = 0; i < (int)hotkeys.size(); i++)
        {
            const CHotkeyEntry &entry = hotkeys[i];
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool enabled = entry.Key != 0;
            ImGui::BeginDisabled(true); // the checkbox mirrors "key assigned" (original check state)
            ImGui::Checkbox("##on", &enabled);
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            // GetHotkeyText(0, ..) renders the "No Key" placeholder
            HotkeyFormatText(entry.Key, entry.Modifiers, s_DataText, sizeof(s_DataText));
            const bool selected = s_SelectedRow == i;
            if (ImGui::Selectable(s_DataText, selected, ImGuiSelectableFlags_SpanAllColumns))
            {
                select_row(i);
            }
            ImGui::TableNextColumn();
            Actions_FormatDataText(entry, s_DataText, sizeof(s_DataText));
            ImGui::TextUnformatted(
                s_DataText[0] != '\0' ? s_DataText : "Blank hotkey"); // m_DefaultBlankHotkeyData
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // Add / Delete / Copy
    if (ImGui::Button("Add"))
    {
        g_AssistantConfig.m_Hotkeys.push_back(CHotkeyEntry());
        select_row((int)g_AssistantConfig.m_Hotkeys.size() - 1);
        g_AssistantConfig.SaveHotkeys();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete"))
    {
        if (s_SelectedRow >= 0 && s_SelectedRow < (int)g_AssistantConfig.m_Hotkeys.size())
        {
            g_AssistantConfig.m_Hotkeys.erase(
                g_AssistantConfig.m_Hotkeys.begin() + s_SelectedRow);
            g_AssistantConfig.SaveHotkeys();
            select_row(-1);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy"))
    {
        // on_copyButton_clicked
        if (s_SelectedRow >= 0 && s_SelectedRow < (int)g_AssistantConfig.m_Hotkeys.size())
        {
            CHotkeyEntry copy = g_AssistantConfig.m_Hotkeys[s_SelectedRow];
            g_AssistantConfig.m_Hotkeys.push_back(copy);
            g_AssistantConfig.SaveHotkeys();
            select_row((int)g_AssistantConfig.m_Hotkeys.size() - 1);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // --- right: the info panel -------------------------------------------
    ImGui::BeginChild("##info", ImVec2(infoWidth, 0.0f));
    ImGui::TextDisabled("Hotkey info");

    // Type combo (on_cb_Type_currentIndexChanged)
    ImGui::SetNextItemWidth(-1.0f);
    {
        static const char *s_TypeItems[HT_LAST] = { nullptr };
        if (s_TypeItems[0] == nullptr)
        {
            for (uint32_t i = 0; i < HT_LAST; i++)
            {
                s_TypeItems[i] = Actions_TypeName(i);
            }
        }
        int typeIndex = (int)s_SelectedType;
        if (ComboGroup("##type", &typeIndex, s_TypeItems, (int)HT_LAST))
        {
            s_SelectedType = (uint32_t)typeIndex;
            if (s_SelectedRow >= 0 && s_SelectedRow < (int)g_AssistantConfig.m_Hotkeys.size())
            {
                g_AssistantConfig.m_Hotkeys[s_SelectedRow].Type = s_SelectedType;
                s_SelectedAction = 0;
                commit_action_selection();
            }
        }
    }

    // Hotkey capture field: click, then press the key/wheel/X
    if (g_Hotkeys.IsCapturing())
    {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(183, 183, 255, 255));
        ImGui::Button("press a key / mouse button (click here to cancel)", ImVec2(-1.0f, 0.0f));
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            g_Hotkeys.CancelCapture();
        }
        // the engine consumed a key/mouse event meanwhile: commit it (CreateKey)
        if (g_Hotkeys.CaptureKey() != 0)
        {
            // false = a duplicate popup opened (the pending assignment rides
            // the engine's popup state); either way the capture buffer resets
            // and keeps listening, like the capture box
            g_Hotkeys.AssignCaptureToRow(s_SelectedRow);
            g_Hotkeys.BeginCapture();
            refresh_hotkey_text();
        }
    }
    else
    {
        if (ImGui::Button(s_HotkeyText, ImVec2(-1.0f, 0.0f)))
        {
            g_Hotkeys.BeginCapture();
        }
    }

    // Action combo
    {
        // Heap-held names (no fixed clamp): the spell/skill lists run to
        // ~450 entries and every one must stay selectable.
        static vector<astr_t> s_ActionItems;
        static uint32_t s_ActionItemsType = 0xFFFFFFFF;
        if (s_ActionItemsType != s_SelectedType)
        {
            s_ActionItemsType = s_SelectedType;
            const int count = Actions_ActionCount(s_SelectedType);
            s_ActionItems.clear();
            s_ActionItems.reserve((size_t)count);
            char name[128];
            for (int i = 0; i < count; i++)
            {
                Actions_ActionName(s_SelectedType, i, name, sizeof(name));
                s_ActionItems.push_back(name);
            }
            if (s_SelectedAction >= count)
            {
                s_SelectedAction = 0; // the list swapped under the selection
            }
        }
        if (!s_ActionItems.empty())
        {
            auto action_getter = [](void *data, int idx) -> const char * {
                return ((vector<astr_t> *)data)->at((size_t)idx).c_str();
            };
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo(
                    "##action", &s_SelectedAction, action_getter, (void *)&s_ActionItems,
                    (int)s_ActionItems.size()))
            {
                commit_action_selection();
            }
        }
        else
        {
            ImGui::TextDisabled("(no action list)");
        }
    }

    // RunMode (cb_RunMode): "Do not auto iterrupt played macro" / "Run one script"
    if (s_SelectedRow >= 0 && s_SelectedRow < (int)g_AssistantConfig.m_Hotkeys.size())
    {
        CHotkeyEntry &entry = g_AssistantConfig.m_Hotkeys[s_SelectedRow];
        const bool scriptKind =
            (entry.Type == HT_SCRIPT || entry.Type == HT_EXTERNAL_CODE);
        if (ImGui::Checkbox(
                scriptKind ? "Run one script" : "Do not auto iterrupt\nplayed macro",
                &entry.RunningMode))
        {
            g_AssistantConfig.SaveHotkeys();
        }
    }

    ImGui::Separator();

    // External code (the CScriptPlainTextEdit stub: plain text this phase;
    // the script engine + highlighter are wired to it)
    ImGui::TextUnformatted("External code:");
    if (s_SelectedRow >= 0 && s_SelectedRow < (int)g_AssistantConfig.m_Hotkeys.size() &&
        g_AssistantConfig.m_Hotkeys[s_SelectedRow].Type == HT_EXTERNAL_CODE &&
        g_AssistantConfig.m_Hotkeys[s_SelectedRow].ExternalCode.empty())
    {
        ImGui::TextDisabled(
            "Lines with the '%s' prefix run as text commands; anything else is inert.",
            g_AssistantConfig.CommandPrefix().c_str());
    }
    if (ImGui::InputTextMultiline(
            "##external_code", s_ExternalCode, sizeof(s_ExternalCode),
            ImVec2(-1.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight() * 2.0f)))
    {
        if (s_SelectedRow >= 0 && s_SelectedRow < (int)g_AssistantConfig.m_Hotkeys.size())
        {
            g_AssistantConfig.m_Hotkeys[s_SelectedRow].ExternalCode = s_ExternalCode;
            g_AssistantConfig.SaveHotkeys();
        }
    }

    if (ImGui::Button("OK"))
    {
        s_FormOpen = false; // on_okButton_clicked
    }

    ImGui::EndChild();
    ImGui::End();
    if (open != nullptr)
    {
        *open = s_FormOpen;
    }
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
