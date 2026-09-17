// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// TabDisplay: the three nested pages - Configuration (the Enabled switch,
// title data + the insert command / set title from group popups), Items
// editor (the custom display items) and Group editor. The data lives in
// g_AssistantConfig (DisplayItems.ini/DisplayGroups.ini/Options.ini display
// section); the compiled title output goes to the OS window caption through
// the manager's Redraw + AssistantUI_Tick. The colored/graphical modes and
// their color/font/icon fields are gone: an OS caption carries no colors,
// images or fonts, so the feature is plain text only.
// Deltas: no drag-reorder list here; the Insert command / set title from
// group actions append the tag at the end of the title data.

#include "assistantui.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include "widgets.h"
#include "../config/assistantconfig.h"
#include "../core/assist.h"
#include "../core/global.h"
#include "../managers/displaymanager.h"

namespace xuoassist {

namespace {

// --- page state ---------------------------------------------------------------
char s_TitleData[2048] = { 0 };   // te_DisplayTitleData
astr_t s_CompiledFrom;            // the data the manager last compiled
bool s_TitleDirty = false;        // a redraw was requested (events / edits)
// the title data field is being edited: an external config change must not
// yank the buffer out from under an active InputText (ImGui's active field
// owns its text until deactivation and would resurrect the pre-edit content)
bool s_TitleDataEditing = false;

int s_SelectedItem = -1;          // lw_DisplayItems
int s_LastSelectedItem = -2;
char s_ItemName[64] = { 0 };      // le_DisplayItemName
char s_ItemText[128] = { 0 };     // le_DisplayItemText
int s_ItemMinValue = 0;           // sb_DisplayItemMinValue
int s_ItemMidValue = 0;           // sb_DisplayItemMidValue
int s_ItemTypeIndex = 0;          // cb_DisplayItemType
char s_Hint[128] = { 0 };

int s_SelectedGroup = -1;         // lw_DisplayGroups
char s_GroupName[64] = { 0 };     // le_DisplayGroupName
char s_GroupData[2048] = { 0 };   // te_DisplayGroupTitleData

const vector<DisplayInfo> &commands()
{
    return g_DisplayManager.GetCommands();
}

// the usable rows of cb_DisplayItemType (Name is empty for separators)
int command_count()
{
    int count = 0;
    for (size_t i = 0; i < commands().size(); i++)
    {
        if (commands()[i].Name.length() != 0)
        {
            count++;
        }
    }
    return count;
}

const DisplayInfo &command_at(int index)
{
    static DisplayInfo empty;
    int count = 0;
    for (size_t i = 0; i < commands().size(); i++)
    {
        if (commands()[i].Name.length() != 0)
        {
            if (count == index)
            {
                return commands()[i];
            }
            count++;
        }
    }
    return empty;
}

// the selected item -> the editor fields
void item_to_fields(const DisplayInfo &item, bool fullUpdate)
{
    if (fullUpdate)
    {
        s_ItemTypeIndex = 0;
        for (size_t i = 0; i < commands().size(); i++)
        {
            if (commands()[i].Type == item.Type && commands()[i].Name.length() != 0)
            {
                const int count = command_count();
                for (int j = 0; j < count; j++)
                {
                    if (command_at(j).Name == commands()[i].Name)
                    {
                        s_ItemTypeIndex = j;
                        break;
                    }
                }
                break;
            }
        }
    }

    snprintf(s_ItemText, sizeof(s_ItemText), "%s", item.Text.c_str());
    s_ItemMidValue = item.MidValue;
    s_ItemMinValue = item.MinValue;
}

// the editor fields -> the entry (+ save + recompile).
void fields_to_item(DisplayItemEntry &entry)
{
    DisplayInfo &di = entry.m_Item;

    const DisplayInfo &info = command_at(s_ItemTypeIndex);
    for (size_t i = 0; i < commands().size(); i++)
    {
        if (commands()[i].Name == info.Name)
        {
            di.Type = commands()[i].Type;
            break;
        }
    }

    di.Text = s_ItemText;
    di.MidValue = s_ItemMidValue;
    di.MinValue = s_ItemMinValue;

    g_AssistantConfig.SaveDisplayItems();
    s_TitleDirty = true; // RecompileData + RedrawTitle
}

bool item_name_in_use(const char *name, int skip)
{
    const vector<DisplayItemEntry> &items = g_AssistantConfig.m_DisplayItems;
    for (int i = 0; i < (int)items.size(); i++)
    {
        if (i != skip && ListNamesEqual(items[i].Name, name))
        {
            return true;
        }
    }
    return false;
}

bool group_name_in_use(const char *name, int skip)
{
    const vector<CDisplayGroupEntry> &groups = g_AssistantConfig.m_DisplayGroups;
    for (int i = 0; i < (int)groups.size(); i++)
    {
        if (i != skip && ListNamesEqual(groups[i].Name, name))
        {
            return true;
        }
    }
    return false;
}

const char *DisplayItemNameOf(const DisplayItemEntry &entry)
{
    return entry.Name.c_str();
}

void DisplayItemSetNameOf(DisplayItemEntry &entry, const char *name)
{
    entry.Name = name;
}

const char *DisplayGroupNameOf(const CDisplayGroupEntry &entry)
{
    return entry.Name.c_str();
}

void DisplayGroupSetNameOf(CDisplayGroupEntry &entry, const char *name)
{
    entry.Name = name;
}

// ask for a caption recomposition
void redraw_title()
{
    s_TitleDirty = true;
}

// Tracks s_CompiledFrom here as
void recompile_title()
{
    s_CompiledFrom = g_AssistantConfig.DisplayTitleData();
    g_DisplayManager.RecompileData(g_AssistantConfig.DisplayTitleData());
    redraw_title();
}

// --- Configuration page ------------------------------------------------------

void draw_configuration_page()
{
    // "Insert command" (pb_DisplayInsertCommand + the context menu)
    if (ImGui::Button("Insert command"))
    {
        ImGui::OpenPopup("##insert_command");
    }
    if (ImGui::BeginPopup("##insert_command"))
    {
        // "Custom items" submenu + the built-in commands
        if (ImGui::BeginMenu("Custom items"))
        {
            const vector<DisplayItemEntry> &items = g_AssistantConfig.m_DisplayItems;
            for (const DisplayItemEntry &item : items)
            {
                if (ImGui::MenuItem(item.Name.c_str()))
                {
                    // the item name is the tag
                    const astr_t tag = "{" + item.Name + "}";
                    const size_t len = strlen(s_TitleData);
                    if (len + tag.length() < sizeof(s_TitleData))
                    {
                        strcat(s_TitleData, tag.c_str());
                        g_AssistantConfig.m_DisplayTitleData = s_TitleData;
                        recompile_title();
                    }
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        for (size_t i = 0; i < commands().size(); i++)
        {
            if (commands()[i].Name.length() != 0 &&
                ImGui::MenuItem(commands()[i].Name.c_str()))
            {
                // the COMMAND is the tag
                const astr_t tag = "{" + commands()[i].Command + "}";
                const size_t len = strlen(s_TitleData);
                if (len + tag.length() < sizeof(s_TitleData))
                {
                    strcat(s_TitleData, tag.c_str());
                    g_AssistantConfig.m_DisplayTitleData = s_TitleData;
                    recompile_title();
                }
            }
        }
        ImGui::EndPopup();
    }

    bool enabled = g_AssistantConfig.DisplayEnabled();
    ImGui::TextUnformatted("Display data on title");
    ImGui::SameLine();
    if (ImGui::Checkbox("Enabled##display", &enabled))
    {
        g_AssistantConfig.m_DisplayEnabled = enabled;
        g_AssistantConfig.SaveMainOptions(); // the display section of Options.ini
        redraw_title();
    }

    // te_DisplayTitleData ("Title data:")
    ImGui::TextUnformatted("Title data:");
    if (ImGui::InputTextMultiline(
            "##title_data", s_TitleData, sizeof(s_TitleData), ImVec2(-1.0f, 120.0f)))
    {
        // recompile + redraw; the
        g_AssistantConfig.m_DisplayTitleData = s_TitleData;
        AssistantUI_MarkOptionsDirty();
        recompile_title();
    }
    s_TitleDataEditing = ImGui::IsItemActive();

    // "Set title from group" (pb_SetTitleGroup + its context menu)
    if (ImGui::Button("Set title from group"))
    {
        ImGui::OpenPopup("##set_title_group");
    }
    if (ImGui::BeginPopup("##set_title_group"))
    {
        const vector<CDisplayGroupEntry> &groups = g_AssistantConfig.m_DisplayGroups;
        for (const CDisplayGroupEntry &group : groups)
        {
            if (ImGui::MenuItem(group.Name.c_str()))
            {
                // -> SetTitleData(text)
                snprintf(s_TitleData, sizeof(s_TitleData), "%s", group.TitleData.c_str());
                g_AssistantConfig.m_DisplayTitleData = s_TitleData;
                AssistantUI_MarkOptionsDirty();
                recompile_title();
            }
        }
        ImGui::EndPopup();
    }
}

// --- Items editor page --------------------------------------------------------

void draw_items_editor_page()
{
    vector<DisplayItemEntry> &items = g_AssistantConfig.m_DisplayItems;

    const int buttons = ListButtons("display_items", XALB_TARGET);

    // the list pane left, the editor fields right
    ImGui::BeginChild("##display_items_pane", ImVec2(100.0f, 0.0f), ImGuiChildFlags_None);
    NameList("##display_items", items, &s_SelectedItem, &DisplayItemNameOf,
             &DisplayItemSetNameOf, false, -1.0f);
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##display_item_form", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    if (s_SelectedItem >= (int)items.size())
    {
        s_SelectedItem = -1;
    }

    // selection -> fields (on_lw_DisplayItems_clicked)
    if (s_SelectedItem != s_LastSelectedItem)
    {
        s_LastSelectedItem = s_SelectedItem;
        s_Hint[0] = '\0';
        if (s_SelectedItem >= 0 && s_SelectedItem < (int)items.size())
        {
            snprintf(s_ItemName, sizeof(s_ItemName), "%s", items[s_SelectedItem].Name.c_str());
            item_to_fields(items[s_SelectedItem].m_Item, true);
        }
    }

    ImGui::TextUnformatted("Properties");
    ImGui::Separator();

    if (ImGui::BeginCombo("Type##display_item", command_at(s_ItemTypeIndex).Name.c_str()))
    {
        const int count = command_count();
        for (int i = 0; i < count; i++)
        {
            if (ImGui::Selectable(command_at(i).Name.c_str(), i == s_ItemTypeIndex))
            {
                s_ItemTypeIndex = i;
                // load the
                item_to_fields(g_DisplayManager.GetDisplayItemByName(command_at(i).Command), false);
            }
        }
        ImGui::EndCombo();
    }

    bool changed = false;
    ImGui::TextUnformatted("Text:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    changed |= ImGui::InputText("##item_text", s_ItemText, sizeof(s_ItemText));

    // "Value:" (sb_DisplayItemMin/MidValue, max 99999 in the .ui)
    ImGui::TextUnformatted("Value:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::InputInt("Minimum##item", &s_ItemMinValue))
    {
        s_ItemMinValue = s_ItemMinValue < 0 ? 0 : (s_ItemMinValue > 99999 ? 99999 : s_ItemMinValue);
        changed = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::InputInt("Middle##item", &s_ItemMidValue))
    {
        s_ItemMidValue = s_ItemMidValue < 0 ? 0 : (s_ItemMidValue > 99999 ? 99999 : s_ItemMidValue);
        changed = true;
    }

    // Name: field (le_DisplayItemName)
    ImGui::TextUnformatted("Name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##item_name", s_ItemName, sizeof(s_ItemName));

    // Create / Save / Remove (the pb_DisplayItem* icon buttons)
    if ((buttons & XALB_ADD) != 0)
    {
        if (s_ItemName[0] == '\0')
        {
            snprintf(s_Hint, sizeof(s_Hint), "Enter the display item name!");
        }
        else if (item_name_in_use(s_ItemName, -1))
        {
            snprintf(s_Hint, sizeof(s_Hint), "Display item name is already exists!");
        }
        else
        {
            DisplayItemEntry entry;
            entry.Name = s_ItemName;
            // push BEFORE the save: the new entry must already sit in the
            // vector when SaveDisplayItems writes the file (fields_to_item's
            // own save is not the only one - kept explicit here)
            items.push_back(entry);
            s_SelectedItem = s_LastSelectedItem = (int)items.size() - 1;
            fields_to_item(items[s_SelectedItem]); // fields -> the entry + marks the title dirty
            g_AssistantConfig.SaveDisplayItems();
            s_Hint[0] = '\0';
        }
    }
    if ((buttons & XALB_SAVE) != 0)
    {
        if (s_ItemName[0] == '\0')
        {
            snprintf(s_Hint, sizeof(s_Hint), "Enter the display item name!");
        }
        else if (s_SelectedItem < 0 || s_SelectedItem >= (int)items.size())
        {
            snprintf(s_Hint, sizeof(s_Hint), "Item is not selected!");
        }
        else if (item_name_in_use(s_ItemName, s_SelectedItem))
        {
            snprintf(s_Hint, sizeof(s_Hint), "Display item name is already exists (not this item)!");
        }
        else
        {
            items[s_SelectedItem].Name = s_ItemName;
            fields_to_item(items[s_SelectedItem]);
            s_Hint[0] = '\0';
        }
    }
    if ((buttons & XALB_REMOVE) != 0 && s_SelectedItem >= 0 && s_SelectedItem < (int)items.size())
    {
        // save + recompile
        items.erase(items.begin() + s_SelectedItem);
        s_SelectedItem = s_LastSelectedItem = -1;
        g_AssistantConfig.SaveDisplayItems();
        recompile_title();
    }
    if (changed && s_SelectedItem >= 0 && s_SelectedItem < (int)items.size())
    {
        g_AssistantConfig.SaveDisplayItems();
        s_TitleDirty = true;
    }
    if (s_Hint[0] != '\0')
    {
        ImGui::TextDisabled("%s", s_Hint);
    }
    ImGui::EndChild();
}

// --- Group editor page ---------------------------------------------------------

void draw_group_editor_page()
{
    vector<CDisplayGroupEntry> &groups = g_AssistantConfig.m_DisplayGroups;

    const int buttons = ListButtons("display_groups", XALB_TARGET);

    // the list pane left, the editor fields right
    ImGui::BeginChild("##display_groups_pane", ImVec2(100.0f, 0.0f), ImGuiChildFlags_None);
    NameList("##display_groups", groups, &s_SelectedGroup, &DisplayGroupNameOf,
             &DisplayGroupSetNameOf, false, -1.0f);
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##display_group_form", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    if (s_SelectedGroup >= (int)groups.size())
    {
        s_SelectedGroup = -1;
    }

    // selection -> fields (on_lw_DisplayGroups_clicked)
    static int lastSelected = -2;
    if (s_SelectedGroup != lastSelected)
    {
        lastSelected = s_SelectedGroup;
        if (s_SelectedGroup >= 0 && s_SelectedGroup < (int)groups.size())
        {
            snprintf(s_GroupName, sizeof(s_GroupName), "%s", groups[s_SelectedGroup].Name.c_str());
            snprintf(s_GroupData, sizeof(s_GroupData), "%s", groups[s_SelectedGroup].TitleData.c_str());
        }
    }

    ImGui::TextUnformatted("Name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##group_name", s_GroupName, sizeof(s_GroupName));
    ImGui::TextUnformatted("Title data:");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextMultiline("##group_data", s_GroupData, sizeof(s_GroupData), ImVec2(-1.0f, 90.0f));

    if ((buttons & XALB_ADD) != 0)
    {
        if (s_GroupName[0] == '\0')
        {
            snprintf(s_Hint, sizeof(s_Hint), "Enter the title group name!");
        }
        else if (group_name_in_use(s_GroupName, -1))
        {
            snprintf(s_Hint, sizeof(s_Hint), "Display group name is already exists!");
        }
        else
        {
            CDisplayGroupEntry entry;
            entry.Name = s_GroupName;
            entry.TitleData = s_GroupData;
            groups.push_back(entry);
            s_SelectedGroup = (int)groups.size() - 1;
            g_AssistantConfig.SaveDisplayGroups();
            recompile_title();
            s_Hint[0] = '\0';
        }
    }
    if ((buttons & XALB_SAVE) != 0)
    {
        if (s_GroupName[0] == '\0')
        {
            snprintf(s_Hint, sizeof(s_Hint), "Enter the title group name!");
        }
        else if (s_SelectedGroup < 0 || s_SelectedGroup >= (int)groups.size())
        {
            snprintf(s_Hint, sizeof(s_Hint), "Group is not selected!");
        }
        else if (group_name_in_use(s_GroupName, s_SelectedGroup))
        {
            snprintf(s_Hint, sizeof(s_Hint), "Display group name is already exists (not this item)!");
        }
        else
        {
            groups[s_SelectedGroup].Name = s_GroupName;
            groups[s_SelectedGroup].TitleData = s_GroupData;
            g_AssistantConfig.SaveDisplayGroups();
            recompile_title();
            s_Hint[0] = '\0';
        }
    }
    if ((buttons & XALB_REMOVE) != 0 && s_SelectedGroup >= 0 && s_SelectedGroup < (int)groups.size())
    {
        groups.erase(groups.begin() + s_SelectedGroup);
        s_SelectedGroup = -1;
        g_AssistantConfig.SaveDisplayGroups();
        recompile_title();
    }
    if (s_Hint[0] != '\0')
    {
        ImGui::TextDisabled("%s", s_Hint);
    }
    ImGui::EndChild();
}

} // namespace

void TabDisplay_Draw()
{
    // keep the compile state in sync with the config (profile load/switch)
    const astr_t key = g_AssistantConfig.DisplayTitleData();
    if (key != s_CompiledFrom)
    {
        s_CompiledFrom = key;
        if (!s_TitleDataEditing)
        {
            snprintf(s_TitleData, sizeof(s_TitleData), "%s", g_AssistantConfig.DisplayTitleData().c_str());
        }
        recompile_title();
    }
    if (ImGui::BeginTabBar("##tw_display", ImGuiTabBarFlags_None))
    {
        if (BeginSubTab("Configuration", "##display_config"))
        {
            draw_configuration_page();
            EndSubTab();
        }
        if (BeginSubTab("Items editor", "##display_items_page"))
        {
            draw_items_editor_page();
            EndSubTab();
        }
        if (BeginSubTab("Group editor", "##display_groups_page"))
        {
            draw_group_editor_page();
            EndSubTab();
        }
        ImGui::EndTabBar();
    }
}

bool TabDisplay_ConsumeTitleDirty()
{
    if (!s_TitleDirty)
    {
        return false;
    }
    s_TitleDirty = false;
    return true;
}

void TabDisplay_RequestTitleRedraw()
{
    s_TitleDirty = true;
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
