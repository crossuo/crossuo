// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The Text Dialog window (see textdialog.h).

#define LOGGER_MODULE Assistant

#include "textdialog.h"

#if defined(XUO_ASSISTANT)

#include <common/logging/logging.h>

#include "../managers/promptmanager.h"
#include "widgets.h"

namespace xuoassist {

namespace {

bool s_Open = false;

// the pte_Text working buffer (the Qt dialog was one plain-text-edit; the
// port keeps a fixed buffer - the draw path never allocates)
constexpr int TEXT_BUFFER_SIZE = 64 * 1024;
char s_Text[TEXT_BUFFER_SIZE] = { 0 };

} // namespace

void TextDialog_Open()
{
    s_Open = true;
}

void TextDialog_Shutdown()
{
    s_Open = false;
    s_Text[0] = '\0';
}

void TextDialog_Draw()
{
    if (!s_Open)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(457.0f, 473.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Text Dialog", &s_Open))
    {
        ImGui::End();
        return;
    }

    // the full-area plain-text-edit (textdialog.ui pte_Text, NoWrap)
    ImGui::InputTextMultiline(
        "##text",
        s_Text,
        TEXT_BUFFER_SIZE,
        ImVec2(-1.0f, -ImGui::GetFrameHeightWithSpacing() * 2.0f),
        ImGuiInputTextFlags_AllowTabInput);

    // the prompt state line (the Qt dialog had no such line; this surfaces
    // where Send/Cancel land - the response flow, not the display flow)
    if (g_PromptManager.GetExists())
    {
        ImGui::Text(
            "Prompt %s pending (serial 0x%08X).",
            g_PromptManager.GetIsUnicode() ? "unicode" : "ascii",
            g_PromptManager.GetSerial());
    }

    // Send/Cancel -> g_PromptManager.Send (packets CPacket*PromptResponse;
    // an empty send is the cancel, promptmanager.cpp Send)
    if (ImGui::Button("Send"))
    {
        if (g_PromptManager.GetExists())
        {
            g_PromptManager.Send(s_Text);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        if (g_PromptManager.GetExists())
        {
            g_PromptManager.Send("");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        s_Text[0] = '\0';
    }

    ImGui::End();
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
