// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// TabAgentsShop (was assist/tabagentsshop.ui/.cpp + shoplisteditorform +
// shopprogressform, §2.8/§3.1): the shop lists (ShopList.ini), the vendor
// name / Buy / Sell / Auto switches, the delay/price/stack/timeout spinboxes,
// the Shop List Editor window (my list <=> vendor list) and the shopping
// progress popup. The ENGINE behind the buttons lives in core/shopagent
// (frame-tick driven); this file is the thin UI over its state.

#include "assistantui.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include "widgets.h"
#include "../config/assistantconfig.h"
#include "../core/assist.h"
#include "../core/shopagent.h"

namespace xuoassist {

namespace {

int s_SelectedList = -1;
int s_LastSelectedList = -1;
char s_ListName[64] = { 0 };
char s_VendorName[64] = { 0 };
char s_Hint[128] = { 0 };
int s_VendorSelected = -1; // the editor's vendor-list selection

// --- the Shop List Editor window state (was ShopListEditorForm) -------------
bool s_EditorOpen = false;
int s_EditedList = -1; // index into m_ShopLists (was m_UsedList)
bool s_EditChanged = false;
int s_EditSelected = -1;
vector<CShopItemEntry> s_EditItems; // the working copy (was lw_ShopList rows)
bool s_EditorExitRequested = false; // pending "Save changes?" decision

char s_EdGraphic[16] = { 0 };
char s_EdColor[16] = { 0 };
char s_EdName[64] = { 0 };
char s_EdComment[64] = { 0 };
int s_EdCount = 0;
int s_EdPrice = 0;

const char *ShopListNameOf(const CShopListEntry &entry)
{
    return entry.Name.c_str();
}

void ShopListSetNameOf(CShopListEntry &entry, const char *name)
{
    entry.Name = name;
}

bool name_in_use(const char *name, int skip)
{
    const vector<CShopListEntry> &lists = g_AssistantConfig.m_ShopLists;
    for (int i = 0; i < (int)lists.size(); i++)
    {
        if (i != skip && ListNamesEqual(lists[i].Name, name))
        {
            return true;
        }
    }
    return false;
}

// was ShopListEditorForm::AddNewItem (+ the duplicate check)
void editor_add_item(const CShopItemEntry &item)
{
    if (item.Graphic.length() == 0)
    {
        snprintf(s_Hint, sizeof(s_Hint), "Enter the shop item graphic!");
        return;
    }
    if (item.Color.length() == 0)
    {
        snprintf(s_Hint, sizeof(s_Hint), "Enter the shop item color!");
        return;
    }

    for (size_t i = 0; i < s_EditItems.size(); i++)
    {
        const CShopItemEntry &entry = s_EditItems[i];
        if (ListNamesEqual(entry.Graphic, item.Graphic.c_str()) &&
            ListNamesEqual(entry.Color, item.Color.c_str()) &&
            (item.Name.length() != 0 && ListNamesEqual(entry.Name, item.Name.c_str())))
        {
            snprintf(s_Hint, sizeof(s_Hint), "Shop item is already exists!");
            return;
        }
    }

    s_EditItems.push_back(item);
    s_EditChanged = true;
    s_EditSelected = (int)s_EditItems.size() - 1;
}

void editor_selection_changed()
{
    // was on_lw_ShopList_clicked: load the row into the editor fields
    if (s_EditSelected >= 0 && s_EditSelected < (int)s_EditItems.size())
    {
        const CShopItemEntry &item = s_EditItems[s_EditSelected];
        snprintf(s_EdGraphic, sizeof(s_EdGraphic), "%s", item.Graphic.c_str());
        snprintf(s_EdColor, sizeof(s_EdColor), "%s", item.Color.c_str());
        snprintf(s_EdName, sizeof(s_EdName), "%s", item.Name.c_str());
        snprintf(s_EdComment, sizeof(s_EdComment), "%s", item.Comment.c_str());
        s_EdCount = item.Count;
        s_EdPrice = item.Price;
    }
}

// was slot_ShopItemSave: field edits update the selected row live
void editor_fields_to_row()
{
    if (s_EditSelected < 0 || s_EditSelected >= (int)s_EditItems.size())
    {
        return;
    }
    CShopItemEntry &item = s_EditItems[s_EditSelected];
    item.Graphic = s_EdGraphic;
    item.Color = s_EdColor;
    item.Name = s_EdName;
    item.Comment = s_EdComment;
    item.Count = s_EdCount;
    item.Price = s_EdPrice;
    s_EditChanged = true;
}

// was ShopListEditorForm::InitList (simplified: no unsaved-changes prompt on
// switching - the fields commit live, the Exit path asks)
void editor_init(int listIndex)
{
    s_EditedList = listIndex;
    s_EditItems.clear();
    s_EditChanged = false;
    s_EditSelected = -1;
    s_EdGraphic[0] = s_EdColor[0] = s_EdName[0] = s_EdComment[0] = '\0';
    s_EdCount = s_EdPrice = 0;

    if (listIndex >= 0 && listIndex < (int)g_AssistantConfig.m_ShopLists.size())
    {
        const vector<CShopItemEntry> &items = g_AssistantConfig.m_ShopLists[listIndex].m_Items;
        s_EditItems.assign(items.begin(), items.end());
        s_EditorOpen = true;
    }
    else
    {
        s_EditorOpen = false;
    }
}

// was on_pb_SaveChanges_clicked
void editor_save_changes()
{
    if (!s_EditChanged || s_EditedList < 0 ||
        s_EditedList >= (int)g_AssistantConfig.m_ShopLists.size())
    {
        return;
    }

    vector<CShopItemEntry> &list = g_AssistantConfig.m_ShopLists[s_EditedList].m_Items;
    list.assign(s_EditItems.begin(), s_EditItems.end());
    s_EditChanged = false;

    g_AssistantConfig.SaveShopList();
    g_Assistant.ClientPrint("Shop list saved!");
}

void draw_shop_list_editor()
{
    if (!s_EditorOpen)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(771.0f, 601.0f), ImGuiCond_FirstUseEver);
    const bool visible = ImGui::Begin("Shop List Editor", &s_EditorOpen);
    if (!visible)
    {
        // window X pressed (was closeEvent -> on_pb_Exit_clicked)
        if (s_EditChanged)
        {
            s_EditorOpen = true; // keep open until the question is answered
            s_EditorExitRequested = true;
            ImGui::OpenPopup("Save changes?");
        }
        else
        {
            editor_init(-1);
        }
    }
    else
    {
        const vector<CVendorShopItem> &vendorItems = g_ShopAgent.GetVendorItems();

        // the three columns: my list | copy button | vendor list
        ImGui::BeginChild("##editor_mine", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() * 5));
        char header[96];
        if (s_EditedList >= 0 && s_EditedList < (int)g_AssistantConfig.m_ShopLists.size())
        {
            snprintf(header, sizeof(header), "List '%s':", g_AssistantConfig.m_ShopLists[s_EditedList].Name.c_str());
        }
        else
        {
            snprintf(header, sizeof(header), "My list:");
        }
        ImGui::TextUnformatted(header);
        ImGui::BeginChild("##editor_mine_list", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        ImGuiListClipper clipper;
        clipper.Begin((int)s_EditItems.size());
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                const CShopItemEntry &item = s_EditItems[i];
                char row[256];
                snprintf(
                    row,
                    sizeof(row),
                    "%s %s %s %i %i %s",
                    item.Graphic.c_str(),
                    item.Color.c_str(),
                    item.Name.c_str(),
                    item.Count,
                    item.Price,
                    item.Comment.c_str());
                ImGui::PushID(i);
                if (ImGui::Selectable(row, s_EditSelected == i))
                {
                    s_EditSelected = i;
                    editor_selection_changed();
                }
                // Delete key removal (was keyPressEvent on lw_ShopList)
                if (s_EditSelected == i && ImGui::IsItemHovered() &&
                    !ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantTextInput &&
                    ImGui::IsKeyChordPressed(ImGuiKey_Delete))
                {
                    s_EditItems.erase(s_EditItems.begin() + i);
                    s_EditSelected = -1;
                    s_EditChanged = true;
                    ImGui::PopID();
                    break; // rows moved: leave the clipper pass for this frame
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::EndChild();

        ImGui::SameLine();

        // center: the copy column (pb_CopyShopItem "<<")
        ImGui::BeginChild("##editor_copy", ImVec2(60.0f, -ImGui::GetFrameHeightWithSpacing() * 5));
        if (ImGui::Button("<<", ImVec2(-1.0f, 0.0f)))
        {
            // was on_pb_CopyShopItem_clicked: vendor item -> my list with
            // count/price reset to 0
            if (s_VendorSelected >= 0 && s_VendorSelected < (int)vendorItems.size())
            {
                const CVendorShopItem &v = vendorItems[s_VendorSelected];
                CShopItemEntry item;
                item.Graphic = CAssistant::GraphicToText(v.Graphic);
                item.Color = CAssistant::GraphicToText(v.Color);
                item.Name = v.Name;
                item.Comment = "";
                item.Count = 0;
                item.Price = 0;
                editor_add_item(item);
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Copy vendor item to shop list,\nor double click on item in vendor list");
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // vendor list (lw_VendorList; refreshed by the agent on every list)
        ImGui::BeginChild("##editor_vendor", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() * 5));
        ImGui::TextUnformatted("Vendor list:");
        ImGui::BeginChild("##editor_vendor_list", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        ImGuiListClipper vclipper;
        vclipper.Begin((int)vendorItems.size());
        while (vclipper.Step())
        {
            for (int i = vclipper.DisplayStart; i < vclipper.DisplayEnd; i++)
            {
                const CVendorShopItem &v = vendorItems[i];
                char row[256];
                snprintf(
                    row,
                    sizeof(row),
                    "%s %s %s %u %u",
                    CAssistant::GraphicToText(v.Graphic).c_str(),
                    CAssistant::GraphicToText(v.Color).c_str(),
                    v.Name.c_str(),
                    v.Count,
                    v.Price);
                ImGui::PushID(i);
                if (ImGui::Selectable(row, s_VendorSelected == i))
                {
                    s_VendorSelected = i;
                }
                if (s_VendorSelected == i && ImGui::IsMouseDoubleClicked(0) &&
                    ImGui::IsItemHovered())
                {
                    // was on_lw_VendorList_doubleClicked -> copy
                    CShopItemEntry item;
                    item.Graphic = CAssistant::GraphicToText(v.Graphic);
                    item.Color = CAssistant::GraphicToText(v.Color);
                    item.Name = v.Name;
                    item.Comment = "";
                    item.Count = 0;
                    item.Price = 0;
                    editor_add_item(item);
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::EndChild();

        // the editor fields (Name/Graphic/Color/Price/Count/Comment)
        bool fieldChanged = false;
        ImGui::SetNextItemWidth(120.0f);
        fieldChanged |= ImGui::InputText("Name", s_EdName, sizeof(s_EdName));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        fieldChanged |= HexEditField("Graphic", s_EdGraphic, sizeof(s_EdGraphic));
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("-1/0xFFFF ignores graphic");
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        fieldChanged |= HexEditField("Color", s_EdColor, sizeof(s_EdColor));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::InputInt("Price", &s_EdPrice))
        {
            s_EdPrice = s_EdPrice < 0 ? 0 : (s_EdPrice > 9999999 ? 9999999 : s_EdPrice);
            fieldChanged = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Buy max price / sell min price\n0 = ignore (always accept)");
        }
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::InputInt("Count", &s_EdCount))
        {
            s_EdCount = s_EdCount < 0 ? 0 : (s_EdCount > 99999 ? 99999 : s_EdCount);
            fieldChanged = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("0 = all");
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        fieldChanged |= ImGui::InputText("Comment", s_EdComment, sizeof(s_EdComment));
        if (fieldChanged)
        {
            editor_fields_to_row();
        }

        // add/remove + Save changes + Exit
        if (ImGui::Button("Add"))
        {
            // was on_pb_ShopItemAdd_clicked
            CShopItemEntry item;
            item.Graphic = s_EdGraphic;
            item.Color = s_EdColor;
            item.Name = s_EdName;
            item.Comment = s_EdComment;
            item.Count = s_EdCount;
            item.Price = s_EdPrice;
            editor_add_item(item);
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove") && s_EditSelected >= 0 &&
            s_EditSelected < (int)s_EditItems.size())
        {
            // was on_pb_ShopItemRemove_clicked
            s_EditItems.erase(s_EditItems.begin() + s_EditSelected);
            s_EditSelected = -1;
            s_EditChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save changes"))
        {
            editor_save_changes();
        }
        ImGui::SameLine();
        if (ImGui::Button("Exit"))
        {
            s_EditorExitRequested = true;
            if (s_EditChanged)
            {
                ImGui::OpenPopup("Save changes?");
            }
            else
            {
                editor_init(-1);
            }
        }
        if (s_Hint[0] != '\0')
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", s_Hint);
        }
    }

    // the unsaved-changes question
    if (s_EditorExitRequested && ImGui::BeginPopupModal("Save changes?", nullptr))
    {
        ImGui::TextUnformatted("You have unsaved changes in this list.\nSave changes?");
        if (ImGui::Button("Yes"))
        {
            editor_save_changes();
            s_EditorExitRequested = false;
            editor_init(-1);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No"))
        {
            s_EditorExitRequested = false;
            editor_init(-1);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    else if (s_EditorExitRequested && !s_EditChanged)
    {
        s_EditorExitRequested = false;
    }

    ImGui::End();
}

// --- the progress popup (was ShopProgressForm) -------------------------------

void draw_shop_progress()
{
    if (!g_ShopAgent.ProgressActive() || !g_ShopAgent.ProgressShow())
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(400.0f, 80.0f), ImGuiCond_Always);
    if (!ImGui::Begin("Shopping progress", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    const int max = g_ShopAgent.ProgressMax();
    const int value = g_ShopAgent.ProgressValue();
    char label[32];
    snprintf(label, sizeof(label), "%d / %d ms", value, max);
    ImGui::ProgressBar(max > 0 ? (float)value / (float)max : 0.0f, ImVec2(-1.0f, 0.0f), label);

    if (ImGui::Button("Cancel shopping!", ImVec2(-1.0f, 0.0f)))
    {
        g_ShopAgent.CancelProgress();
    }

    ImGui::End();
}

} // namespace

// the progress popup as an always-on UI hook: AssistantUI_Draw calls this
// outside the tab pages, so the popup renders while the agent is active with
// any tab (or none) visible (draw_shop_progress itself early-outs otherwise).
// External linkage on purpose: declared in assistantui.h, called from
// assistantui.cpp (an anonymous-namespace definition never links).
void TabShop_DrawProgress()
{
    draw_shop_progress();
}

void TabShop_Draw()
{
    vector<CShopListEntry> &lists = g_AssistantConfig.m_ShopLists;

    ImGui::TextUnformatted("Shop lists:");
    const int buttons = ListButtons("shop");
    bool changed = NameList(
        "##shop_lists", lists, &s_SelectedList, &ShopListNameOf, &ShopListSetNameOf, false, 160.0f);
    if (changed)
    {
        g_AssistantConfig.SaveShopList();
    }
    if (s_SelectedList >= (int)lists.size())
    {
        s_SelectedList = -1;
    }

    // selection -> "List name:" (on_lw_ShopLists_clicked). Delta: the original
    // app also opened the editor on a list double click; the Edit shop list
    // items button is the route here (the list widget is a child window and
    // double clicks cannot be attributed to rows through it)
    if (s_SelectedList != s_LastSelectedList)
    {
        s_LastSelectedList = s_SelectedList;
        s_Hint[0] = '\0';
        if (s_SelectedList >= 0 && s_SelectedList < (int)lists.size())
        {
            snprintf(s_ListName, sizeof(s_ListName), "%s", lists[s_SelectedList].Name.c_str());
        }
    }

    if ((buttons & XALB_ADD) != 0)
    {
        // was on_pb_ShopListCreate_clicked
        if (s_ListName[0] == '\0')
        {
            snprintf(s_Hint, sizeof(s_Hint), "Enter the shop list name!");
        }
        else if (name_in_use(s_ListName, -1))
        {
            snprintf(s_Hint, sizeof(s_Hint), "Shop list name is already exists!");
        }
        else
        {
            CShopListEntry entry;
            entry.Name = s_ListName;
            lists.push_back(entry);
            s_SelectedList = s_LastSelectedList = (int)lists.size() - 1;
            g_AssistantConfig.SaveShopList();
            s_Hint[0] = '\0';
        }
    }
    if ((buttons & XALB_SAVE) != 0)
    {
        // was on_pb_ShopListSave_clicked
        if (s_ListName[0] == '\0')
        {
            snprintf(s_Hint, sizeof(s_Hint), "Enter the shop list name!");
        }
        else if (s_SelectedList < 0 || s_SelectedList >= (int)lists.size())
        {
            snprintf(s_Hint, sizeof(s_Hint), "Name is not selected!");
        }
        else if (name_in_use(s_ListName, s_SelectedList))
        {
            snprintf(s_Hint, sizeof(s_Hint), "Shop list name is already exists (not this item)!");
        }
        else
        {
            lists[s_SelectedList].Name = s_ListName;
            g_AssistantConfig.SaveShopList();
            s_Hint[0] = '\0';
        }
    }
    if ((buttons & XALB_REMOVE) != 0 && s_SelectedList >= 0 && s_SelectedList < (int)lists.size())
    {
        // was on_pb_ShopListRemove_clicked (a bound editor view is dropped)
        if (s_EditedList == s_SelectedList)
        {
            editor_init(-1);
        }
        lists.erase(lists.begin() + s_SelectedList);
        s_SelectedList = s_LastSelectedList = -1;
        g_AssistantConfig.SaveShopList();
    }

    ImGui::TextUnformatted("List name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    ImGui::InputText("##shop_list_name", s_ListName, sizeof(s_ListName));
    ImGui::TextUnformatted("Vendor name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::InputText("##shop_vendor_name", s_VendorName, sizeof(s_VendorName)))
    {
        g_ShopAgent.SetVendorName(s_VendorName);
    }

    ImGui::Separator();

    // Buy / Sell with the Auto switches (cb_AutoBuy/SellSelectedList)
    bool autoBuy = g_ShopAgent.AutoBuy();
    if (ImGui::Checkbox("Auto##buy", &autoBuy))
    {
        g_ShopAgent.SetAutoBuy(autoBuy);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Automatically buy selected shop list.");
    }
    ImGui::SameLine();
    bool autoSell = g_ShopAgent.AutoSell();
    if (ImGui::Checkbox("Auto##sell", &autoSell))
    {
        g_ShopAgent.SetAutoSell(autoSell);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Automatically sell selected shop list.");
    }

    const bool haveList = s_SelectedList >= 0 && s_SelectedList < (int)lists.size();
    ImGui::BeginDisabled(!haveList);
    if (ImGui::Button("Buy"))
    {
        // was on_pb_BuySelectedList_clicked
        g_ShopAgent.Buy(lists[s_SelectedList].Name);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Buy selected shop list.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Sell"))
    {
        // was on_pb_SellSelectedList_clicked
        g_ShopAgent.Sell(lists[s_SelectedList].Name);
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    // the spinbox block (sb_ShopDelay / sb_MaxSellPrice / sb_MaxShopItems /
    // sb_ShopTimeout / cb_ShowShoppingProgressBar)
    struct XA_SHOP_SPIN
    {
        const char *Label;
        int *Value;
        int Min, Max;
    };
    XA_SHOP_SPIN spins[] = {
        { "Shop delay for 1 stack in list (ms):", &g_AssistantConfig.m_ShopDelay, 1, 9999 },
        { "Max total sell price (0 - unlimited):", &g_AssistantConfig.m_MaxSellPrice, 0, 9999999 },
        { "Max shop stacks (0 - unlimited):", &g_AssistantConfig.m_MaxShopItems, 0, 999 },
        { "Timeout if shop is not recv. (sec):", &g_AssistantConfig.m_ShopTimeout, 1, 300 },
    };
    bool dirty = false;
    for (int i = 0; i < int(sizeof(spins) / sizeof(spins[0])); i++)
    {
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::InputInt(spins[i].Label, spins[i].Value))
        {
            *spins[i].Value = *spins[i].Value < spins[i].Min
                ? spins[i].Min
                : (*spins[i].Value > spins[i].Max ? spins[i].Max : *spins[i].Value);
            dirty = true;
        }
    }
    bool showProgress = g_AssistantConfig.m_ShowShoppingProgressBar;
    if (ImGui::Checkbox("Show shopping progress bar", &showProgress))
    {
        g_AssistantConfig.m_ShowShoppingProgressBar = showProgress;
        dirty = true;
    }
    if (dirty)
    {
        g_AssistantConfig.SaveShopList(); // ShopList.ini data attributes
    }

    if (ImGui::Button("Edit shop list items"))
    {
        // was on_pb_EditShopListItems_clicked
        editor_init(haveList ? s_SelectedList : -1);
    }
    if (s_Hint[0] != '\0')
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", s_Hint);
    }

    // the Shop List Editor window (separate window, gated by this draw like
    // the hotkeys control forms); the progress popup is drawn from
    // AssistantUI_Draw (TabShop_DrawProgress) so it stays up while the shop
    // agent runs with any tab visible
    draw_shop_list_editor();
}

void TabShop_CloseEditor()
{
    // disarm any open editor without touching the agent state (shutdown)
    editor_init(-1);
    s_EditorExitRequested = false;
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
