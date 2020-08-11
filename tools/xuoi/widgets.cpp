#include "widgets.h"

void HoverToolTip(const char *desc)
{
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetTextLineHeightWithSpacing() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void HelpMarker(const char *desc)
{
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGui::GetColorU32(ImGuiCol_Text));
    ImGui::TextDisabled("" ICON_FK_QUESTION_CIRCLE);
    ImGui::PopStyleColor();
    HoverToolTip(desc);
}

void InputText(
    const char *id,
    const char *label,
    float w,
    char *buf,
    size_t buf_size,
    ImGuiInputTextFlags flags,
    ImGuiInputTextCallback callback,
    void *user_data)
{
    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::PushItemWidth(w);
    ImGui::InputText(id, buf, buf_size, flags, callback, user_data);
    ImGui::PopItemWidth();
}

bool ComboBox(
    const char *id,
    const char *label,
    float w,
    int *current_item,
    const char *const items[],
    int items_count,
    int height_in_items)
{
    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::PushItemWidth(w);
    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_SelectedEntryBg));
    const bool changed = ImGui::Combo(id, current_item, items, items_count, height_in_items);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    return changed;
}

bool ComboBox(
    const char *id,
    const char *label,
    float w,
    int *current_item,
    bool (*items_getter)(void *data, int idx, const char **out_text),
    void *data,
    int items_count,
    int height_in_items = -1)
{
    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::PushItemWidth(w);
    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_SelectedEntryBg));
    const bool changed =
        ImGui::Combo(id, current_item, items_getter, data, items_count, height_in_items);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    return changed;
}

// copied from imgui_widgets
bool ListBox(
    const char *label,
    int *current_item,
    bool (*items_getter)(void *, int, const char **),
    void *data,
    int items_count,
    int height_in_items,
    ListBoxItemCallback callback)
{
    if (!ImGui::ListBoxHeader(label, items_count, height_in_items))
        return false;

    ImGuiSelectableFlags flags = 0;
    if (callback)
    {
        flags = ImGuiSelectableFlags_AllowDoubleClick;
    }

    // Assume all items have even height (= 1 line of text). If you need items of different or variable sizes you can create a custom version of ListBox() in your code without using the clipper.
    bool value_changed = false;
    ImGuiListClipper clipper(
        items_count,
        ImGui::
            GetTextLineHeightWithSpacing()); // We know exactly our line height here so we pass it as a minor optimization, but generally you don't need to.
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            const bool item_selected = (i == *current_item);
            const char *item_text;
            if (!items_getter(data, i, &item_text))
                item_text = "*Unknown item*";

            ImGui::PushID(i);
            if (ImGui::Selectable(item_text, item_selected, flags))
            {
                *current_item = i;
                value_changed = true;
                if (callback && ImGui::IsMouseDoubleClicked(0))
                {
                    callback(i);
                }
            }
            if (item_selected)
                ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
    ImGui::ListBoxFooter();
    return value_changed;
}
