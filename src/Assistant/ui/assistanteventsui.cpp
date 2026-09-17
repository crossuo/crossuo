// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The UI events consumer: journal ring buffer + the journal pane.

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include "assistanteventsui.h"
#include "assistantui.h" // TabDisplay_RequestTitleRedraw / AssistantUI_RequestClientClose
#include "widgets.h"
#include "../config/assistantconfig.h" // g_AssistantConfig (Autostart gate)
#include "../core/actions.h"
#include "../macros/macroexecutor.h"
#include "../managers/guildmanager.h"
#include "../script/journal.h" // the script journal mirror feed
#include "../script/scriptstate.h" // the script buff mirror feed
#include "../core/world.h"
#include "Party.h" // ::g_Party (shared party data)
#include "mapwindow.h"
#include "inspector.h"

namespace xuoassist {

static CAssistantUIEvents s_UIEvents;

CAssistantUIEvents *AssistantUIEvents_Get()
{
    return &s_UIEvents;
}

void CAssistantUIEvents::OnBuffAdd(uint16_t graphic, uint32_t timer, const astr_t &name)
{
    // the script-visible buff mirror;
    ScriptBuffs_Add(graphic, timer, name);
}

void CAssistantUIEvents::OnBuffRemove(uint16_t graphic)
{
    ScriptBuffs_Remove(graphic);
}

void CAssistantUIEvents::OnJournalMessage(const CJournalMessage &message)
{
    // the script-visible mirror (script/journal.cpp) shares the same ring
    // ring semantics
    ScriptJournal_Add(message);
    // copy the payload, cap the ring
    Line line;
    line.Text = message.Text;
    line.Color = message.Color;
    line.Flags = message.Flags;
    m_Lines.push_back(line);
    while ((int)m_Lines.size() > LINE_CAP)
    {
        m_Lines.erase(m_Lines.begin());
    }
}

void CAssistantUIEvents::OnSpellList(const vector<vector<astr_t>> &books)
{
    // feed the spell hotkey action list
    Actions_SetSpellList(books);
}

void CAssistantUIEvents::OnSkillsList(const vector<astr_t> &hotkeyActions)
{
    // feed the skill hotkey action list
    Actions_SetSkillList(hotkeyActions);
}

void CAssistantUIEvents::OnRecordTargetObject(uint32_t serial, uint16_t graphic, uint16_t color)
{
    // record a CMacroWithObject action
    g_MacroExecutor.RecordTargetObject(serial, graphic, color);
}

void CAssistantUIEvents::OnRecordTargetTile(uint16_t graphic, int x, int y, int z)
{
    // record a CMacroWithTargetTile action
    g_MacroExecutor.RecordTargetTile(graphic, x, y, z);
}

void CAssistantUIEvents::OnAutostartRequest()
{
    // the checkbox
    if (g_AssistantConfig.Autostart())
    {
        AssistantUI_QueueAutostart();
    }
}

void CAssistantUIEvents::OnClientCloseRequest()
{
    // the Wisp window lives in assistantui.cpp (header conflicts with the
    // xuocore event structs keep it out of this TU)
    AssistantUI_RequestClientClose();
}

void CAssistantUIEvents::OnFromTarget(
    int kind, uint32_t serial, uint16_t graphic, uint16_t color, const astr_t &name)
{
    // the HandleTarget* forwards: park the latest reply for the list tabs
    XA_FROM_TARGET_RESULT &slot = FromTargetResult();
    slot.Kind = kind;
    slot.Serial = serial;
    slot.Graphic = graphic;
    slot.Color = color;
    slot.Name = name;
    slot.Generation++;
}

XA_FROM_TARGET_RESULT &CAssistantUIEvents::FromTargetResult()
{
    static XA_FROM_TARGET_RESULT s_Result;
    return s_Result;
}

bool UI_PollFromTarget(
    bool &pending, uint32_t &generation, int kind, XA_FROM_TARGET_RESULT &out)
{
    if (!pending)
    {
        return false;
    }
    XA_FROM_TARGET_RESULT &slot = CAssistantUIEvents::FromTargetResult();
    if (slot.Generation == generation || slot.Kind != kind)
    {
        return false;
    }
    generation = slot.Generation;
    out = slot;
    return true;
}

void CAssistantUIEvents::OnTitleRedraw()
{
    // recompose + apply the caption on change
    TabDisplay_RequestTitleRedraw();
}

void CAssistantUIEvents::OnTitleRedraw(uint32_t serial)
{
    // the manager gates on
    (void)serial;
    TabDisplay_RequestTitleRedraw();
}

// --- map + inspector events ------------------------------------------------------------------

void CAssistantUIEvents::OnMapUpdate(uint32_t serial)
{
    // the overlays are recomputed from the
    (void)serial;
    MapWindow_OnMapUpdate();
}

void CAssistantUIEvents::OnMapRedraw()
{
    // request a map redraw
    MapWindow_OnMapRedraw();
}

void CAssistantUIEvents::OnMapOpenRequest()
{
    // open the map window (ShowOnLogin / the hotkey
    MapWindow_Open();
}

void CAssistantUIEvents::OnTrack(bool state, int x, int y)
{
    // the tracker point; the
    MapWindow_OnTrack(state, x, y);
}

void CAssistantUIEvents::OnInspectorUpdate(uint32_t serial)
{
    // refresh the inspector sections (the
    Inspector_OnInspectorUpdate(serial);
}

void CAssistantUIEvents::OnPartyMemberInfo(const XA_PARTY_MEMBER_INFO &info)
{
    // Krrios packet positions for party members outside the client's view
    // range; stored in the shared party data for the map window
    g_Party.SetMemberInfo(info.Serial, info.X, info.Y, (uint8_t)info.Map, info.Name);
}

void CAssistantUIEvents::OnGuildMemberInfo(const XA_GUILD_MEMBER_INFO &info)
{
    GUILD_MEMBER_INFO stored;
    stored.Serial = info.Serial;
    stored.X = info.X;
    stored.Y = info.Y;
    stored.Map = info.Map;
    stored.Name = info.Name;
    g_GuildManager.SetMemberInfo(stored);
}

void JournalPane_Draw(float height)
{
    const CAssistantUIEvents *events = AssistantUIEvents_Get();
    const vector<CAssistantUIEvents::Line> &lines = events->Lines();

    ImGui::BeginChild("##journal", ImVec2(0.0f, height), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("Journal:");
    ImGui::Separator();
    if (g_World == nullptr)
    {
        // not logged in: the assistant has no journal feed yet
        ImGui::TextDisabled("(journal is empty - not logged in)");
    }
    else if (lines.empty())
    {
        ImGui::TextDisabled("(no assistant journal messages)");
    }
    else
    {
        ImGui::BeginChild("##journal_lines", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
        ImGuiListClipper clipper;
        clipper.Begin((int)lines.size());
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                const CAssistantUIEvents::Line &line = lines[i];
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(UOColorToRGBA(line.Color)));
                ImGui::TextUnformatted(line.Text.c_str());
                ImGui::PopStyleColor();
            }
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f)
        {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
