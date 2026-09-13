// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// TabAgentsDress (was assist/tabagentsdress.ui/.cpp, §2.8): the dress sets
// list plus the working items list, the apply/dress/undress buttons and the
// two option checkboxes. The set data lives in g_AssistantConfig.m_DressSets
// (Dress.coa, saved on every mutation); the sequences run through
// core/dressagent (the original app generated a throwaway script instead).

#include "assistantui.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include "widgets.h"
#include "assistanteventsui.h"
#include "../config/assistantconfig.h"
#include "../core/assist.h"
#include "../core/dressagent.h"
#include "../core/global.h"
#include "../model/gameobjects/gameitem.h"
#include "../model/gameobjects/gameplayer.h" // g_Player
#include "../model/gameobjects/gameworld.h"

namespace xuoassist {

namespace {

// the original tab kept a WORKING COPY of the selected set's items in the
// lw_DressItems widget (from-target / apply / clear mutated the copy; Create
// and Save committed the copy into a set) - s_WorkRows mirrors that
int s_SelectedSet = -1;
int s_SelectedItem = -1;
char s_Name[64] = { 0 };
char s_Hint[128] = { 0 };

// the from-target arm state (see tablists.cpp for the pattern)
bool s_FtPending = false;
uint s_FtGeneration = 0;

const char *DressNameOf(const CDressSetEntry &entry)
{
    return entry.Name.c_str();
}

void DressSetNameOf(CDressSetEntry &entry, const char *name)
{
    entry.Name = name;
}

// the items-in-set rows: "<layer name> = 0x########" (was
// CDressItemListItem::UpdateText); items carry no name field, the label is
// derived from the layer slot
struct XA_DRESS_ITEM_ROW
{
    int Layer;
    uint Serial;
};

const int s_WorkRowMax = 25;
XA_DRESS_ITEM_ROW s_WorkRows[s_WorkRowMax];
int s_WorkRowCount = 0;

void work_from_set(const CDressSetEntry &set)
{
    s_WorkRowCount = 0;
    for (int layer = 0; layer < 25 && s_WorkRowCount < s_WorkRowMax; layer++)
    {
        if (set.m_Items[layer] != 0u)
        {
            s_WorkRows[s_WorkRowCount].Layer = layer;
            s_WorkRows[s_WorkRowCount].Serial = set.m_Items[layer];
            s_WorkRowCount++;
        }
    }
}

void work_into_set(CDressSetEntry &set)
{
    memset(&set.m_Items[0], 0, sizeof(set.m_Items));
    for (int i = 0; i < s_WorkRowCount; i++)
    {
        const int layer = s_WorkRows[i].Layer;
        if (layer > 0 && layer < 25)
        {
            set.m_Items[layer] = s_WorkRows[i].Serial;
        }
    }
}

// was AddDressItemInList(list, layer, serial): replace the same-layer entry
void work_add_item(int layer, uint serial)
{
    if (layer <= 0 || layer >= 25)
    {
        return;
    }

    for (int i = 0; i < s_WorkRowCount; i++)
    {
        if (s_WorkRows[i].Layer == layer)
        {
            s_WorkRows[i].Serial = serial;
            return;
        }
    }

    if (s_WorkRowCount < s_WorkRowMax)
    {
        s_WorkRows[s_WorkRowCount].Layer = layer;
        s_WorkRows[s_WorkRowCount].Serial = serial;
        s_WorkRowCount++;
    }
}

bool name_in_use(const char *name, int skipIndex)
{
    const vector<CDressSetEntry> &sets = g_AssistantConfig.m_DressSets;
    for (int i = 0; i < (int)sets.size(); i++)
    {
        if (i != skipIndex && ListNamesEqual(sets[i].Name, name))
        {
            return true;
        }
    }
    return false;
}

// the per-set layer scan buttons (was on_pb_DressSetCurrent/ApplyHands/
// ApplyArmor_clicked): read the player's equipped items from the mirror
// checkSafe matches the original guards: SetCurrent and ApplyArmor required
// g_LayerSafe[layer], ApplyHands did not (tabagentsdress.cpp:187-236)
void apply_from_equipment(int minLayer, int maxLayer, bool checkSafe)
{
    if (g_Player == nullptr || s_SelectedSet < 0 ||
        s_SelectedSet >= (int)g_AssistantConfig.m_DressSets.size())
    {
        return;
    }

    bool changed = false;
    QFOR(item, g_Player->m_Items, CGameItem *)
    {
        const int layer = item->GetEquipLayer();

        if (layer < minLayer || layer > maxLayer)
        {
            continue;
        }

        if (layer == OL_BACKPACK || (checkSafe && !g_LayerSafe[layer]))
        {
            continue;
        }

        // replace/insert into the working copy at the right slot order
        int slot = 0;
        while (slot < s_WorkRowCount && s_WorkRows[slot].Layer < layer)
        {
            slot++;
        }
        if (slot < s_WorkRowCount && s_WorkRows[slot].Layer == layer)
        {
            s_WorkRows[slot].Serial = item->GetSerial();
        }
        else if (s_WorkRowCount < s_WorkRowMax)
        {
            for (int shift = s_WorkRowCount; shift > slot; shift--)
            {
                s_WorkRows[shift] = s_WorkRows[shift - 1];
            }
            s_WorkRows[slot].Layer = layer;
            s_WorkRows[slot].Serial = item->GetSerial();
            s_WorkRowCount++;
        }
        changed = true;
    }

    if (changed)
    {
        work_into_set(g_AssistantConfig.m_DressSets[s_SelectedSet]);
        g_AssistantConfig.SaveDress(); // Dress.coa (the original button saved too)
    }
}

} // namespace

void TabDress_Draw()
{
    vector<CDressSetEntry> &sets = g_AssistantConfig.m_DressSets;
    XA_FROM_TARGET_RESULT ftResult;

    // the "Undress before dressing" / "Use double click for equip items"
    // checkboxes (the original bound them straight to the config accessors)
    bool undressFirst = g_AssistantConfig.UndressBeforeDressing();
    if (ImGui::Checkbox("Undress before dressing", &undressFirst))
    {
        g_AssistantConfig.m_DressUndressBeforeDress = undressFirst;
        g_AssistantConfig.SaveDress();
    }
    ImGui::SameLine();
    bool doubleClick = g_AssistantConfig.DoubleClickForEquipItems();
    if (ImGui::Checkbox("Use double click for equip items", &doubleClick))
    {
        g_AssistantConfig.m_DressDoubleClickForEquipment = doubleClick;
        g_AssistantConfig.SaveDress();
    }

    ImGui::Separator();

    // left: "Dress sets:" (lw_Dress) + the icon row
    ImGui::BeginChild("##dress_sets_pane", ImVec2(240.0f, 0.0f), ImGuiChildFlags_None);
    ImGui::TextUnformatted("Dress sets:");
    const int buttons = ListButtons("dress");
    if ((buttons & XALB_TARGET) != 0)
    {
        // was on_pb_DressFromTarget_clicked
        s_FtPending = true;
        g_TargetHandler = &CAssistant::HandleTargetDressItem;
        g_Assistant.RequestTarget("Select a dress item");
    }
    if (UI_PollFromTarget(s_FtPending, s_FtGeneration, XA_FROM_TARGET_DRESS_ITEM, ftResult))
    {
        // was AddDressItemInList(item): resolve the equip layer through the
        // mirror and replace the working-copy slot
        if (g_World != nullptr)
        {
            CGameItem *item = g_World->FindWorldItem(ftResult.Serial);
            if (item != nullptr)
            {
                work_add_item(item->GetEquipLayer(), item->GetSerial());
            }
        }
    }

    const int beforeCount = (int)sets.size();
    bool changed = NameList(
        "##dress_sets", sets, &s_SelectedSet, &DressNameOf, &DressSetNameOf, false, -1.0f);
    if (changed)
    {
        // rename/reorder/inline-delete: persist (the explicit buttons save too)
        g_AssistantConfig.SaveDress();
    }
    if ((int)sets.size() != beforeCount && s_SelectedSet >= (int)sets.size())
    {
        s_SelectedSet = -1;
    }

    // selection -> the working copy + the name field (on_lw_Dress_clicked)
    static int lastSelected = -2;
    if (s_SelectedSet != lastSelected)
    {
        lastSelected = s_SelectedSet;
        s_Hint[0] = '\0';
        if (s_SelectedSet >= 0 && s_SelectedSet < (int)sets.size())
        {
            snprintf(s_Name, sizeof(s_Name), "%s", sets[s_SelectedSet].Name.c_str());
            work_from_set(sets[s_SelectedSet]);
        }
        else
        {
            s_Name[0] = '\0';
            s_WorkRowCount = 0;
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Name:");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##dress_name", s_Name, sizeof(s_Name));

    // Create (on_pb_DressCreate_clicked)
    if ((buttons & XALB_ADD) != 0)
    {
        if (s_Name[0] == '\0')
        {
            snprintf(s_Hint, sizeof(s_Hint), "Enter the dress name!");
        }
        else if (name_in_use(s_Name, -1))
        {
            snprintf(s_Hint, sizeof(s_Hint), "Dress name is already exists!");
        }
        else
        {
            CDressSetEntry entry;
            entry.Name = s_Name;
            work_into_set(entry);
            sets.push_back(entry);
            s_SelectedSet = lastSelected = (int)sets.size() - 1;
            g_AssistantConfig.SaveDress();
            s_Hint[0] = '\0';
        }
    }
    // Save (on_pb_DressSave_clicked): rename + commit the working copy
    if ((buttons & XALB_SAVE) != 0)
    {
        if (s_Name[0] == '\0')
        {
            snprintf(s_Hint, sizeof(s_Hint), "Enter the dress name!");
        }
        else if (s_SelectedSet < 0 || s_SelectedSet >= (int)sets.size())
        {
            snprintf(s_Hint, sizeof(s_Hint), "Dress set is not selected!");
        }
        else if (name_in_use(s_Name, s_SelectedSet))
        {
            snprintf(s_Hint, sizeof(s_Hint), "Dress name is already exists (not this item)!");
        }
        else
        {
            sets[s_SelectedSet].Name = s_Name;
            work_into_set(sets[s_SelectedSet]);
            g_AssistantConfig.SaveDress();
            s_Hint[0] = '\0';
        }
    }
    // Remove (on_pb_DressRemove_clicked)
    if ((buttons & XALB_REMOVE) != 0 && s_SelectedSet >= 0 && s_SelectedSet < (int)sets.size())
    {
        sets.erase(sets.begin() + s_SelectedSet);
        s_SelectedSet = lastSelected = -1;
        s_Name[0] = '\0';
        s_WorkRowCount = 0;
        g_AssistantConfig.SaveDress();
    }

    if (s_Hint[0] != '\0')
    {
        ImGui::TextDisabled("%s", s_Hint);
    }

    // the sequence buttons (the original column of pb_Dress* buttons)
    if (ImGui::Button("Apply current equipment to set"))
    {
        apply_from_equipment(1, OL_MOUNT - 1, true);
    }
    if (ImGui::Button("Apply weapon and shield to set"))
    {
        apply_from_equipment(1, OL_2_HAND, false);
    }
    if (ImGui::Button("Apply armor to set"))
    {
        apply_from_equipment(OL_2_HAND + 1, OL_MOUNT - 1, true);
    }
    ImGui::Separator();
    ImGui::BeginDisabled(g_DressAgent.Active());
    if (ImGui::Button("Dress selected set"))
    {
        // was on_pb_DressUse_clicked -> DressSet(currentItem); the original app
        // dressed the LIVE set data (not the working copy)
        if (s_SelectedSet >= 0 && s_SelectedSet < (int)sets.size())
        {
            g_DressAgent.DressSet(&sets[s_SelectedSet]);
        }
    }
    ImGui::EndDisabled();
    if (g_DressAgent.Active() && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("A dress sequence is running");
    }
    if (ImGui::Button("Undress"))
    {
        g_DressAgent.Undress(); // was on_pb_DressUndress_clicked -> UndressSet
    }
    if (ImGui::Button("Clear set items"))
    {
        s_WorkRowCount = 0; // was on_pb_DressClearSet_clicked: lw_DressItems->clear()
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // right: "Items in set:" (lw_DressItems, the working copy)
    ImGui::BeginChild("##dress_items_pane", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    ImGui::TextUnformatted("Items in set:");
    ImGui::Separator();
    ImGui::BeginChild("##dress_items", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    for (int i = 0; i < s_WorkRowCount; i++)
    {
        char row[96];
        snprintf(
            row,
            sizeof(row),
            "%s = %s",
            g_LayerName[s_WorkRows[i].Layer].c_str(),
            CAssistant::SerialToText(s_WorkRows[i].Serial).c_str());
        ImGui::PushID(i);
        if (ImGui::Selectable(row, s_SelectedItem == i))
        {
            s_SelectedItem = i;
        }
        ImGui::PopID();
    }
    // Delete-key removal (was OnDeleteKeyClick on lw_DressItems)
    if (s_SelectedItem >= 0 && s_SelectedItem < s_WorkRowCount &&
        !ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantTextInput &&
        ImGui::IsKeyChordPressed(ImGuiKey_Delete))
    {
        for (int shift = s_SelectedItem; shift < s_WorkRowCount - 1; shift++)
        {
            s_WorkRows[shift] = s_WorkRows[shift + 1];
        }
        s_WorkRowCount--;
        s_SelectedItem = -1;
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

void TabDress_CancelPendingFromTargets()
{
    s_FtPending = false;
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
