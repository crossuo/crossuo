// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// TabSkills (was assist/tabskills.ui/.cpp, §2.9): the live stats row, the
// skills table (Name/Base/Real/Delta/Cap), the clipboard copies and the delta
// reset. The data lives in g_Skills[]/g_SkillsCount (fed by the packet
// pipeline: the 0xFC skill list names the rows, 0xDF/0x3A carry the values
// and lock states) - the table reads it live every frame, like the original table
// was updated from UpdateSkill/UpdateStatsAndSkillsInfo.
//
// Delta note: the original delta column showed Value - OldValue, suppressed while
// (int)(delta * 10) == 0; "Reset skills delta" set OldValue = Value for every
// skill (only the deltas are zeroed, no packet is sent). Lock-state icons
// (skill_*.bmp) render as text glyphs with tooltips here; the original table's
// icons were display-only (tabskills.cpp has no cell click handler), so there
// is no click-to-cycle behavior to port.

#include "assistantui.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include <memory.h>
#include <time.h>

#include "widgets.h"
#include "../core/assist.h"
#include "../core/global.h"
#include "../model/commonitems/skill.h"
#include "../model/gameobjects/gameplayer.h"

namespace xuoassist {

namespace {

// the lock-state glyphs for skill_up/down/lock/wtf.bmp (display-only,
// parity with the original: no click behavior) + the tooltip text
struct XA_SKILL_LOCK_STYLE
{
    const char *Glyph;
    const char *Tip;
};

const XA_SKILL_LOCK_STYLE s_LockStyles[] = {
    { "^", "Skill is set to raise" },    // skill_up.bmp
    { "v", "Skill is set to lower" },    // skill_down.bmp
    { "o", "Skill is locked" },          // skill_lock.bmp
    { "?", "Unknown lock state" },       // skill_wtf.bmp
};

const char *skill_lock_style(uchar status)
{
    return s_LockStyles[status > 2 ? 3 : status].Glyph;
}

// the original code compared the packet-derived floats directly (the same ushort
// /10.0f conversion on both sides makes exact equality meaningful); memcmp
// keeps the exact-value semantics without tripping -Wfloat-equal
bool skill_value_equal(float a, float b)
{
    return memcmp(&a, &b, sizeof(a)) == 0;
}

// the report date, "yyyy-MM-dd" (local time)
void report_date(char *buf, size_t size)
{
    const time_t now = time(nullptr);
    strftime(buf, size, "%Y-%m-%d", localtime(&now));
}

} // namespace

void TabSkills_Draw()
{
    // --- the stats info row (l_Str/l_Int/l_Dex/l_Statsum/l_Skillsum) --------
    float skillSum = 0.0f;
    for (int i = 0; i < g_SkillsCount; i++)
    {
        skillSum += g_Skills[i].GetValue();
    }

    int statSum = 0;
    if (g_Player != nullptr)
    {
        statSum = g_Player->GetStr() + g_Player->GetInt() + g_Player->GetDex();
        ImGui::Text(
            "Str: %i   Int: %i   Dex: %i   Statsum: %i   Skillsum: %.1f",
            g_Player->GetStr(),
            g_Player->GetInt(),
            g_Player->GetDex(),
            statSum,
            skillSum);
    }
    else
    {
        // not logged in (g_Skills empty): the original labels showed zeros
        ImGui::Text("Str: 0   Int: 0   Dex: 0   Statsum: 0   Skillsum: %.1f", skillSum);
    }
    ImGui::TextDisabled("Stats info:");
    ImGui::Separator();

    // --- the table (tw_Skills, columns 160/50/50/50/50) ----------------------
    static int selected = -1;

    if (ImGui::BeginTable(
            "##skills", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Real", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Delta", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Cap", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(g_SkillsCount);
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                const CSkill &skill = g_Skills[i];
                const float delta = skill.GetValue() - skill.GetOldValue();

                ImGui::PushID(i);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                char nameCell[192];
                snprintf(
                    nameCell,
                    sizeof(nameCell),
                    "%s  %s",
                    skill_lock_style(skill.GetStatus()),
                    skill.GetName().c_str());
                if (ImGui::Selectable(nameCell, selected == i, ImGuiSelectableFlags_SpanAllColumns))
                {
                    selected = i;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", s_LockStyles[skill.GetStatus() > 2 ? 3 : skill.GetStatus()].Tip);
                }
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", skill.GetBaseValue());
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", skill.GetValue());
                ImGui::TableNextColumn();
                // was UpdateSkill: the delta cell stayed empty while
                // (int)(valueDelta * 10) == 0
                if ((int)(delta * 10) != 0)
                {
                    ImGui::Text("%3.1f", delta);
                }
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", skill.GetCap());
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    // --- the buttons ---------------------------------------------------------
    if (ImGui::Button("Copy selected to clipboard"))
    {
        // was on_bt_CopySelectedToClipboard_clicked
        if (selected >= 0 && selected < g_SkillsCount)
        {
            const CSkill &skill = g_Skills[selected];
            char buf[192];

            if (skill_value_equal(skill.GetValue(), skill.GetOldValue()))
            {
                snprintf(buf, sizeof(buf), "\t%.1f\n", skill.GetValue());
            }
            else
            {
                snprintf(
                    buf,
                    sizeof(buf),
                    "\t%.1f\t %.1f\n",
                    skill.GetValue(),
                    skill.GetValue() - skill.GetOldValue());
            }

            string text = skill.GetName() + buf;
            ImGui::SetClipboardText(text.c_str());
        }
    }
    if (ImGui::IsItemHovered() && (selected < 0 || selected >= g_SkillsCount))
    {
        ImGui::SetTooltip("Click a skill row first");
    }
    ImGui::SameLine();

    if (ImGui::Button("Copy all to clipboard"))
    {
        // was on_bt_CopyAllToClipboard_clicked (format verbatim)
        char date[16] = { 0 };
        report_date(date, sizeof(date));
        string result =
            "===========X:UO Assist===========\n===Skills status report, " + string(date) + "===\n";

        float reportSkillSum = 0.0f;

        for (int i = 0; i < g_SkillsCount; i++)
        {
            const CSkill &skill = g_Skills[i];
            string nameBuf = skill.GetName();

            if (nameBuf.length() < 8)
            {
                nameBuf += "\t\t\t";
            }
            else if (nameBuf.length() < 16)
            {
                nameBuf += "\t\t";
            }
            else
            {
                nameBuf += "\t";
            }

            result += nameBuf;

            reportSkillSum += skill.GetValue();

            char buf[64];
            if (skill_value_equal(skill.GetValue(), skill.GetOldValue()))
            {
                snprintf(buf, sizeof(buf), "%.1f\n", skill.GetValue());
            }
            else
            {
                snprintf(
                    buf,
                    sizeof(buf),
                    "%.1f\t%.1f\n",
                    skill.GetValue(),
                    skill.GetValue() - skill.GetOldValue());
            }

            result += buf;
        }

        char buf[128];
        if (g_Player != nullptr)
        {
            const int statCap = g_Player->GetStr() + g_Player->GetInt() + g_Player->GetDex();
            snprintf(
                buf,
                sizeof(buf),
                "Str %i Int %i Dex %i\n___Skillcap: %.1f Statcap: %i___\n",
                g_Player->GetStr(),
                g_Player->GetInt(),
                g_Player->GetDex(),
                reportSkillSum,
                statCap);
        }
        else
        {
            snprintf(buf, sizeof(buf), "Str 0 Int 0 Dex 0\n___Skillcap: %.1f Statcap: 0___\n", reportSkillSum);
        }

        result += buf;
        ImGui::SetClipboardText(result.c_str());
    }
    ImGui::SameLine();

    if (ImGui::Button("Reset skills delta"))
    {
        // was on_bt_Reset_SkillsDelta_clicked: OldValue = Value for all
        // skills (deltas only - no packet, no server interaction)
        for (int i = g_SkillsCount - 1; i >= 0; i--)
        {
            g_Skills[i].SetOldValue(g_Skills[i].GetValue());
        }
    }
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
