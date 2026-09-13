// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The Phase 2 UI events consumer: journal ring buffer + the journal pane.
// Phase 3 additions: the spell/skill hotkey action lists (core/actions), the
// macro recording taps (macros/macroexecutor) and the from-target replies
// (ui/tablists.cpp polls the latest result slot).

#include "assistanteventsui.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include "assistantui.h" // TabDisplay_RequestTitleRedraw
#include "widgets.h"
#include "../core/actions.h"
#include "../macros/macroexecutor.h"
#include "../model/gameobjects/gameworld.h" // xuoassist::g_World (mirror)

namespace xuoassist {

static CAssistantUIEvents s_UIEvents;

CAssistantUIEvents *AssistantUIEvents_Get()
{
    return &s_UIEvents;
}

void CAssistantUIEvents::OnJournalMessage(const CJournalMessage &message)
{
    // was g_JournalManager.Add: copy the payload, cap the ring
    Line line;
    line.Text = message.GetText();
    line.Color = message.GetColor();
    line.Flags = message.GetFlags();
    m_Lines.push_back(line);
    while ((int)m_Lines.size() > LINE_CAP)
    {
        m_Lines.erase(m_Lines.begin());
    }
}

void CAssistantUIEvents::OnSpellList(const vector<vector<string>> &books)
{
    // was g_HotkeysForm->UpdateActionList(HT_CAST*, hotkeyActions)
    Actions_SetSpellList(books);
}

void CAssistantUIEvents::OnSkillsList(const vector<string> &hotkeyActions)
{
    // was g_HotkeysForm->UpdateActionList(HT_USE_SKILL*, hotkeyActions)
    Actions_SetSkillList(hotkeyActions);
}

void CAssistantUIEvents::OnRecordTargetObject(uint serial, ushort graphic, ushort color)
{
    // was g_TabMacros->AddAction(new CMacroWithObject(...)) (target.cpp:210)
    g_MacroExecutor.RecordTargetObject(serial, graphic, color);
}

void CAssistantUIEvents::OnRecordTargetTile(ushort graphic, int x, int y, int z)
{
    // was g_TabMacros->AddAction(new CMacroWithTargetTile(...)) (target.cpp:190)
    g_MacroExecutor.RecordTargetTile(graphic, x, y, z);
}

void CAssistantUIEvents::OnFromTarget(
    int kind, uint serial, ushort graphic, ushort color, const string &name)
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
    bool &pending, uint &generation, int kind, XA_FROM_TARGET_RESULT &out)
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
    // was g_TabDisplay->RedrawTitle: recompose + apply the caption on change
    TabDisplay_RequestTitleRedraw();
}

void CAssistantUIEvents::OnTitleRedraw(uint serial)
{
    // was g_TabDisplay->CheckRedrawTitle(obj): the manager gated on
    // CanRedraw(graphic, color) before redrawing (packetmanager.cpp:1457)
    (void)serial;
    TabDisplay_RequestTitleRedraw();
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
