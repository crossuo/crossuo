// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include "ui_model.h"
#include "ui_shards.h"
#include "shards.cpp"
#include "common.h"

// view
void HoverToolTip(const char *desc);
bool ComboBox(
    const char *id,
    const char *label,
    float w,
    int *current_item,
    bool (*items_getter)(void *data, int idx, const char **out_text),
    void *data,
    int items_count,
    int height_in_items = -1);

bool ui_shards_combo(
    const char *id, const char *label, float w, int *current_item, int popup_max_height_in_items)
{
    return ComboBox(
        id,
        label,
        w,
        current_item,
        shard_getter,
        &s_shards.entries,
        s_shards.entries.size(),
        popup_max_height_in_items);
}

static int s_selected = 0;
static int s_picked = -1;
const ImVec4 bg_selected = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
const ImVec4 bg_highlighted = ImVec4(0.45f, 0.35f, 0.35f, 1.00f);
const ImVec4 bg_alt = ImVec4(0.23f, 0.23f, 0.23f, 1.00f);
const ImVec4 bg_base = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
const ImVec4 white = ImColor(1.0f, 1.0f, 1.0f, 1.0f);
const ImVec4 yellow = ImColor(1.0f, 1.0f, 0.0f, 1.0f);

int shard_picked()
{
    int r = s_picked;
    s_picked = -1;
    return r;
}

void ui_shards(ui_model &m, bool picker)
{
    const auto line_size = ImGui::GetTextLineHeightWithSpacing();
    const auto entry_size = line_size * 4;
    auto item_size = entry_size;
    ImVec4 bg = bg_base;
    ImVec4 fg = white;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
    static bool select_pvp = false;
    static bool select_pvm = false;
    static bool select_rp = false;
    static bool select_aac = false;
    if (ImGui::TreeNode("Filters"))
    {
        ImGui::Checkbox("PvP  ", &select_pvp);
        HoverToolTip("Player vs. Player enabled shard (PKs/Factions)");
        ImGui::SameLine();
        ImGui::Checkbox("PvM  ", &select_pvm);
        HoverToolTip("Player vs. Player is not permitted");
        ImGui::SameLine();
        ImGui::Checkbox("RP  ", &select_rp);
        HoverToolTip("Roleplay Based");
        ImGui::SameLine();
        ImGui::Checkbox("Auto Account  ", &select_aac);
        HoverToolTip("Auto account creation, just log-in and play");
        ImGui::TreePop();
    }
    const int filler = picker ? -13 : +10;
    const float y = m.area.y - ImGui::GetCursorPosY() + line_size + filler;
    ImGui::BeginChild("shards", { m.area.x, y }, false, window_flags);
    for (int i = 1; i < s_shards.entries.size(); i++)
    {
        const auto &it = s_shards.entries[i];
        const bool has_tags = !it.shard_tags.tags.empty();
        if (has_tags)
        {
            const auto &tags = it.shard_tags;
            if (select_pvp && !tags.is_pvp)
                continue;
            if (select_pvm && tags.is_pvp)
                continue;
            if (select_rp && !tags.is_rp)
                continue;
        }
        // context menu entries
        const bool has_reg = !it.shard_urlregister.empty();
        if (select_aac && !it.shard_urlregister.empty())
            continue;
        const bool has_url = !it.shard_url.empty() && valid_url(it.shard_url);
        const bool has_forum = !it.shard_urlforum.empty() && valid_url(it.shard_urlforum);
        const bool has_other = !it.shard_urlother.url.empty();
        const bool has_install = !it.shard_installer.url.empty();
        const bool has_context = has_url || has_reg || has_forum || has_other || has_install;
        // others
        const bool has_desc = !it.shard_description.empty();
        const bool has_lang = !it.shard_language.langs.empty();

        if (i == s_selected)
        {
            bg = bg_selected;
            item_size = entry_size * 2;
        }
        else
        {
            bg = i % 2 ? bg_alt : bg_base;
            item_size = entry_size;
        }

        if (it.shard_tags.is_highlighted)
        {
            bg = bg_highlighted;
            fg = yellow;
        }
        else
        {
            fg = white;
        }

        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        {
            ImGui::BeginChild("shard_item", ImVec2(0.0f, item_size), true);
            ImGui::BeginGroup();
            ImGui::PushStyleColor(ImGuiCol_Text, fg);
            ImGui::Text("%s", it.shard_name.c_str());
            ImGui::PopStyleColor();
            if (has_url)
            {
                ImGui::SameLine();
                ImGui::Text("%s", it.shard_url.c_str());
            }
            if (i == s_selected && has_desc)
                ImGui::TextWrapped("%s", it.shard_description.c_str());
            if (has_lang)
                ImGui::Text("Language: %s", it.raw_shard_language.c_str());
            if (has_tags)
                ImGui::Text("Tags: %s", it.raw_shard_tags.c_str()); // FIXME: pretty print
            ImGui::EndGroup();
            //if (ImGui::IsItemHovered())
            //    ImGui::SetTooltip("Shard %04d hovered", i);
            ImGui::EndChild();

            if (ImGui::IsItemClicked())
                s_selected = i;

            if (has_context && ImGui::BeginPopupContextItem("item context menu"))
            {
                if (has_url && ImGui::MenuItem("Open Website", nullptr, false))
                    open_url(it.shard_url);
                if (has_reg && valid_url(it.shard_urlregister) &&
                    ImGui::MenuItem("Create Account", nullptr, false))
                    open_url(it.shard_urlregister);
                if (has_forum && ImGui::MenuItem("Open Forum", nullptr, false))
                    open_url(it.shard_urlforum);
                if (has_other && ImGui::MenuItem(it.shard_urlother.name.c_str(), nullptr, false))
                    open_url(it.shard_urlother.url);
                if (has_install)
                {
                    switch (it.shard_installer.type)
                    {
                        case shard::tag_install_type::website:
                        {
                            if (ImGui::MenuItem("View Instructions", nullptr, false))
                                open_url(it.shard_installer.url);
                        }
                        break;
                        case shard::tag_install_type::download_zip:
                        case shard::tag_install_type::download_web:
                        case shard::tag_install_type::download_exe:
                        {
                            if (ImGui::MenuItem("Download Installer", nullptr, false))
                                open_url(it.shard_installer.url);
                        }
                        break;
                        case shard::tag_install_type::xuo_patch_server:
                        {
                            if (ImGui::MenuItem("Install Now (not implemented)", nullptr, false))
                                ;
                        }
                        break;
                        case shard::tag_install_type::uo_patch_server:
                        case shard::tag_install_type::uo_latest:
                        {
                            if (ImGui::MenuItem("Download Now", nullptr, false))
                                open_url(
                                    "http://web.cdn.eamythic.com/us/uo/installers/20120309/UOClassicSetup_7_0_24_0.exe");
                        }
                        break;
                    }
                }
                // by knowing all our data paths and data version, we can match supported shards
                // and enable a quick join feature where no account profile setup is required
                //if (!has_reg && ImGui::MenuItem("Quick Join", nullptr, false))
                //    ;
                ImGui::EndPopup();
            }
        }
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (picker)
    {
        ImGui::NewLine();
        ImGui::SameLine(m.area.x - 160.0f);
        if (ImGui::Button("Pick Selected"))
        {
            LOG_TRACE("pick shard: %d", s_selected);
            s_picked = s_selected;
            ui_pop(m);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ui_pop(m);
    }
}
