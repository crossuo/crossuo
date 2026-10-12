// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Launcher

#include "ui_model.h"
#include "ui_shards.h"
#include "accounts.h"
#include "shards.cpp"
#include <xuocore/common.h>

namespace xuol {

// view
void HoverToolTip(const char *desc);
bool ComboBox(
    const char *id,
    const char *label,
    float w,
    int *current_item,
    const char *(*items_getter)(void *data, int idx),
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
        int(s_shards.entries.size()),
        popup_max_height_in_items);
}

static int s_selected = 0;
static int s_picked = -1;

// menu label for a plain shardother community link
static const char *shardother_label(const astr_t &url)
{
    return url_is_discord(url) ? "Open Discord" : "Open Link";
}
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
    // both footers (the picker's Pick/Cancel and the tab's Play) reserve a row
    const int filler = -13;
    const float y = m.area.y - ImGui::GetCursorPosY() + line_size + filler;
    ImGui::BeginChild("shards", { m.area.x, y }, false, window_flags);
    for (int i = 1; i < (int)s_shards.entries.size(); i++)
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
        // autoacct (default yes) is the authority on auto-account: shardaccount
        // is now independent - it is a page to reach for account creation OR
        // management, offered whenever it is set
        const bool autoacct = it.shard_autoacct;
        const bool has_reg = !it.shard_shardaccount.empty();
        if (select_aac && !autoacct)
            continue;
        const bool has_url = !it.shard_shardweb.empty() && valid_url(it.shard_shardweb);
        const bool has_forum = !it.shard_shardcommunity.empty() && valid_url(it.shard_shardcommunity);
        const bool has_other = !it.shard_shardother.empty() && valid_url(it.shard_shardother);
        const bool has_context = has_url || has_reg || has_forum || has_other;
        // others
        const bool has_desc = !it.shard_description.empty();
        const bool has_lang = !it.raw_shard_language.empty();

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
            ImGui::Text("%s", it.shard_shardname.c_str());
            ImGui::PopStyleColor();
            if (has_url)
            {
                ImGui::SameLine();
                ImGui::Text("%s", it.shard_shardweb.c_str());
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

            // select on either button: the context menu below acts on
            // s_selected, so a right click must move the selection to the row
            // under the cursor rather than leaving it on a previous one
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
                ImGui::IsItemClicked(ImGuiMouseButton_Right))
                s_selected = i;

            // double-click launches the shard right away (tab mode only: the
            // picker confirms the selection with its own footer button)
            if (!picker && ImGui::IsItemClicked(ImGuiMouseButton_Left) &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                account_launch_shard(i);

            if (has_context && ImGui::BeginPopupContextItem("item context menu"))
            {
                if (has_url && ImGui::MenuItem("Open Website", nullptr, false))
                    open_url(it.shard_shardweb);
                // the launcher's "Create account" action follows autoacct: a
                // non-auto shard needs the registration page; an auto one may
                // still link a page to MANAGE the account
                if (has_reg && valid_url(it.shard_shardaccount) &&
                    ImGui::MenuItem(autoacct ? "Manage Account" : "Create Account", nullptr, false))
                    open_url(it.shard_shardaccount);
                if (has_forum && ImGui::MenuItem("Open Forum", nullptr, false))
                    open_url(it.shard_shardcommunity);
                if (has_other &&
                    ImGui::MenuItem(shardother_label(it.shard_shardother), nullptr, false))
                    open_url(it.shard_shardother);
                // installer is a streaming data product url the client
                // consumes via Play: no menu action (empty = manual install)
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
    else
    {
        // footer: a hint for the selected shard on the left, the launch
        // action on the right (the picker has its own Pick/Cancel footer)
        const bool can_play = s_selected > 0 && s_selected < (int)s_shards.entries.size() &&
                              !s_shards.entries[s_selected].shard_loginserver.empty();
        ImGui::NewLine();
        if (can_play)
            ImGui::TextDisabled("Login: %s", s_shards.entries[s_selected].shard_loginserver.c_str());
        ImGui::SameLine(m.area.x - 160.0f);
        ImGui::BeginDisabled(!can_play);
        if (ImGui::Button("Play"))
        {
            LOG_TRACE("launch shard: %d", s_selected);
            account_launch_shard(s_selected);
        }
        ImGui::EndDisabled();
    }
}

} // namespace xuol
