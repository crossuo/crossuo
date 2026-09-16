// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// TabHotkeys implementation (see tabhotkeys.h). Was assist/tabhotkeys.cpp +
// the three control forms; every button keeps its original speech payload.

#define LOGGER_MODULE Assistant

#include "tabhotkeys.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include <stdio.h>

#include "assistanteventsui.h" // CAssistantUIEvents::FromTargetResult
#include "hotkeysform.h"
#include "widgets.h"
#include "../config/assistantconfig.h"
#include "../core/assist.h"
#include "../core/assistantevents.h"
#include "../core/global.h"
#include "../core/hotkeys.h"
#include "../core/packets.h"
#include "TextEngine/GameConsole.h" // ::SendConsoleText (the control-form commands)

namespace xuoassist {

namespace {

bool s_FormOpen = false;
bool s_AnimalControlOpen = false;
bool s_HouseControlOpen = false;
bool s_ShipControlOpen = false;
char s_AnimalName[64] = "all"; // le_Name default ("all")

// The "Name from target" arm state (from_target_poll pattern, tablists.cpp:
// 76): armed by the target button, polled on the next frame, cancellable from
// CancelPendingFromTargets (shutdown).
bool s_AnimalFtPending = false;
uint s_AnimalFtGeneration = 0;

// AnimalAndVendorControlForm::Send
void animal_send(const char *command)
{
    string text = string(s_AnimalName) + " " + command;
    wstring wide;
    wide.reserve(text.size());
    for (char c : text)
    {
        wide.push_back((wchar_t)(uchar)c);
    }
    SendConsoleText(wide, 0);
}

// ShipControlForm::Send
void ship_send(const char *text)
{
    wstring wide;
    wide.reserve(strlen(text));
    for (const char *c = text; *c != '\0'; ++c)
    {
        wide.push_back((wchar_t)(uchar)*c);
    }
    SendConsoleText(wide, 0);
}

void house_send(const wchar_t *text)
{
    // HouseControlForm ctor buttons
    SendConsoleText(text, 0);
}

bool speech_button(const char *label, float width = 0.0f)
{
    return ImGui::Button(label, ImVec2(width < 1.0f ? 0.0f : width, 0.0f));
}

void draw_animal_control()
{
    // AnimalAndVendorControlForm: the Animals grid + the Vendors grid
    if (!ImGui::Begin("Animal and vendor control", &s_AnimalControlOpen))
    {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted("Animals:");
    const char *const animals[4][3] = {
        { "Come", "Go", "Follow" },
        { "Stay", "Guard", "Stop" },
        { "Transfer", "Release", "Attack" },
        { "Drop", "Give", "Kill" },
    };
    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 3; col++)
        {
            if (col != 0)
            {
                ImGui::SameLine();
            }
            if (speech_button(animals[row][col], 80.0f))
            {
                animal_send(animals[row][col]);
            }
        }
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Vendors:");
    const char *const vendors[2][3] = {
        { "Buy", "Bye", "Stock" },
        { "Sell", "Hire", "Price" },
    };
    for (int row = 0; row < 2; row++)
    {
        for (int col = 0; col < 3; col++)
        {
            if (col != 0)
            {
                ImGui::SameLine();
            }
            if (speech_button(vendors[row][col], 80.0f))
            {
                animal_send(vendors[row][col]);
            }
        }
    }
    ImGui::SameLine();
    if (speech_button("Status", 80.0f))
    {
        animal_send("Status");
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputText("##animal_name", s_AnimalName, sizeof(s_AnimalName));
    ImGui::SameLine();
    if (IconButton(XA_ICON_TARGET, "Type from target"))
    {
        // AnimalAndVendorControlForm::on_pb_NameFromTarget_clicked
        g_TargetHandler = &CAssistant::HandleTargetAnimalAndVendorControlName;
        g_Assistant.RequestTarget("Select a object for obtain name");
        s_AnimalFtPending = true;
    }
    // The reply parked in the OnFromTarget slot: fill the name field on the
    // next frame (the poll half of the tablists.cpp from_target pattern).
    if (s_AnimalFtPending)
    {
        const XA_FROM_TARGET_RESULT &slot = CAssistantUIEvents::FromTargetResult();
        if (slot.Generation != s_AnimalFtGeneration &&
            slot.Kind == XA_FROM_TARGET_ANIMAL_NAME)
        {
            s_AnimalFtGeneration = slot.Generation;
            snprintf(s_AnimalName, sizeof(s_AnimalName), "%s", slot.Name.c_str());
            s_AnimalFtPending = false;
        }
    }
    ImGui::End();
}

void draw_house_control()
{
    if (!ImGui::Begin("House control", &s_HouseControlOpen))
    {
        ImGui::End();
        return;
    }
    struct XA_HOUSE_BUTTON
    {
        const char *Label;
        const wchar_t *Speech;
    };
    const XA_HOUSE_BUTTON buttons[] = {
        { "Lock down", L"I wish to lock this down" },
        { "Secure", L"I wish to secure this" },
        { "Release", L"I wish to release this" },
        { "Ban", L"I ban thee" },
        { "Trash", L"I wish to place a trash barrel" },
        { "Remove", L"Remove thyself" },
        { "Strongbox", L"I wish to place a strongbox" },
    };
    for (const XA_HOUSE_BUTTON &button : buttons)
    {
        if (ImGui::Button(button.Label, ImVec2(120.0f, 0.0f)))
        {
            house_send(button.Speech);
        }
    }
    ImGui::End();
}

void draw_ship_control()
{
    if (!ImGui::Begin("Ship control", &s_ShipControlOpen))
    {
        ImGui::End();
        return;
    }
    // cb_MoveState: Normal / One / Slow prefixes the movement commands
    static int moveState = 0;
    static const char *const states[] = { "Normal", "One", "Slow" };
    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("##movestate", &moveState, states, 3);
    ImGui::Separator();

    ImGui::TextUnformatted("Anchors");
    if (speech_button("Raise", 90.0f))
    {
        ship_send("Raise Anchor");
    }
    ImGui::SameLine();
    if (speech_button("Drop", 90.0f))
    {
        ship_send("Drop Anchor");
    }
    ImGui::TextUnformatted("Turning");
    if (speech_button("<", 60.0f))
    {
        ship_send("Turn Left");
    }
    ImGui::SameLine();
    if (speech_button("^", 60.0f))
    {
        ship_send("Turn Around");
    }
    ImGui::SameLine();
    if (speech_button(">", 60.0f))
    {
        ship_send("Turn Right");
    }
    ImGui::TextUnformatted("Moving");
    struct XA_SHIP_MOVE
    {
        const char *Label;
        const char *Speech;
    };
    const XA_SHIP_MOVE moves[3][3] = {
        { { "\\|", "Forward Left" }, { "|", "Forward" }, { "|/", "Forward Right" } },
        { { "<", "Left" }, { "[]", "Stop" }, { ">", "Right" } },
        { { "/", "Backward Left" }, { "/", "Backward" }, { "\\=", "Backward Right" } },
    };
    for (int row = 0; row < 3; row++)
    {
        for (int col = 0; col < 3; col++)
        {
            if (col != 0)
            {
                ImGui::SameLine();
            }
            if (speech_button(moves[row][col].Label, 60.0f))
            {
                if (strcmp(moves[row][col].Speech, "Stop") == 0)
                {
                    ship_send("Stop"); // Stop never takes the state prefix
                }
                else if (moveState == 0)
                {
                    ship_send(moves[row][col].Speech);
                }
                else
                {
                    string text = string(states[moveState]) + " " + moves[row][col].Speech;
                    ship_send(text.c_str());
                }
            }
        }
    }
    ImGui::End();
}

} // namespace

void TabHotkeys_Draw()
{
    bool hotkeysOn = g_AssistantConfig.OnOffHotkeys();
    if (ImGui::Checkbox("On/Off hotkeys", &hotkeysOn))
    {
        // on_cb_OnOffHotkeys_clicked + SetOnOffHotkeys
        if (g_AssistantConfig.m_OnOffHotkeys != hotkeysOn)
        {
            g_AssistantConfig.m_OnOffHotkeys = hotkeysOn;
            g_AssistantConfig.SaveMainOptions();
        }
        g_AssistantEvents->OnClientPrint(
            hotkeysOn ? "Hotkeys now is enabled" : "Hotkeys now is disabled");
    }

    bool passHotkeys = g_AssistantConfig.PassHotkeys();
    if (ImGui::Checkbox("Pass keys to client", &passHotkeys))
    {
        g_AssistantConfig.m_PassHotkeys = passHotkeys;
        g_AssistantConfig.SaveMainOptions();
    }

    // Command prefix (le_CommandPrefix, default "_")
    static char prefix[16] = { 0 };
    static bool prefixLoaded = false;
    if (!prefixLoaded)
    {
        snprintf(prefix, sizeof(prefix), "%s", g_AssistantConfig.CommandPrefix().c_str());
        prefixLoaded = true;
    }
    ImGui::TextUnformatted("Command prefix:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    if (ImGui::InputText("##prefix", prefix, sizeof(prefix)))
    {
        g_AssistantConfig.m_CommandPrefix = prefix;
        g_AssistantConfig.SaveMainOptions();
    }

    ImGui::Separator();

    // Load/Save (tabhotkeys.cpp:53/77): the original app opened a file dialog under
    // SavedHotkeys; the in-client port round-trips the profile Hotkeys.xml
    // (the format is identical - see core/hotkeys.cpp)
    if (ImGui::Button("Load hotkeys"))
    {
        if (g_AssistantConfig.LoadHotkeys())
        {
            g_Assistant.ClientPrint("Hotkeys loaded.");
        }
        else
        {
            g_Assistant.ClientPrint("Hotkeys load failed (no profile file).");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save hotkeys"))
    {
        g_AssistantConfig.SaveHotkeys();
        g_Assistant.ClientPrint("Hotkeys saved.");
    }
    ImGui::SameLine();
    if (IconButton(XA_ICON_HOTKEY, "Open hotkeys dialog"))
    {
        s_FormOpen = true; // on_pb_OpenHotkeysDialog_clicked
    }

    ImGui::Separator();

    // the control launchers
    if (ImGui::Button("Animal and vendor control"))
    {
        s_AnimalControlOpen = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("House control"))
    {
        s_HouseControlOpen = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Ship control"))
    {
        s_ShipControlOpen = true;
    }
}

void TabHotkeys_DrawControlForms()
{
    if (s_FormOpen)
    {
        HotkeysForm_Draw(&s_FormOpen);
    }
    if (s_AnimalControlOpen)
    {
        draw_animal_control();
    }
    if (s_HouseControlOpen)
    {
        draw_house_control();
    }
    if (s_ShipControlOpen)
    {
        draw_ship_control();
    }
}

void TabHotkeys_CancelPendingFromTargets()
{
    s_AnimalFtPending = false;
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
