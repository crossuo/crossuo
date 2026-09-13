// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The Phase 2 UI events consumer: journal ring buffer + the journal pane.

#include "assistanteventsui.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include "widgets.h"
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
