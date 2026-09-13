// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// TabAgentsParty (was assist/tabagentsparty.ui/.cpp, §2.8): the accept /
// decline lists (Name/Serial/from-target) and the four auto-response
// checkboxes. The DECISION logic itself is packet-driven and already lives in
// managers/partymanager.cpp (OnPacketReceived case 7 -> config
// PartyInviteCanBeAutoAccepted/AutoDeclined) - this tab edits the very
// structures it consults (Party.xml, saved on every mutation).

#include "assistantui.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include "widgets.h"
#include "assistanteventsui.h"
#include "../config/assistantconfig.h"
#include "../core/assist.h"

namespace xuoassist {

namespace {

// shared serial/name fields (the original le_PartySerial/le_PartyName pair served
// both lists)
char s_Name[64] = { 0 };
char s_Serial[16] = { 0 };
char s_Hint[128] = { 0 };
bool s_FtPending = false;
uint s_FtGeneration = 0;

const char *PartyNameOf(const CWorldObjectEntry &entry)
{
    return entry.Name.c_str();
}

void PartySetNameOf(CWorldObjectEntry &entry, const char *name)
{
    entry.Name = name;
}

// the original tabs compared CAssistant::TextToSerial(serial) != 0
uint serial_from_field()
{
    return CAssistant::TextToSerial(s_Serial);
}

bool serial_in_use(const vector<CWorldObjectEntry> &items, uint serial, int skip)
{
    for (int i = 0; i < (int)items.size(); i++)
    {
        if (i != skip && items[i].Serial == serial)
        {
            return true;
        }
    }
    return false;
}

// the add/save/remove flows of the two lists are identical except for the
// target container (was on_pb_PartyAutoAccept*/PartyAutoDecline*_clicked)
void list_add(vector<CWorldObjectEntry> &items, bool &changed)
{
    const uint serial = serial_from_field();
    if (serial == 0u)
    {
        snprintf(s_Hint, sizeof(s_Hint), "Enter the object serial!");
        return;
    }
    if (serial_in_use(items, serial, -1))
    {
        snprintf(s_Hint, sizeof(s_Hint), "Object serial is already exists!");
        return;
    }
    CWorldObjectEntry entry;
    entry.Serial = serial;
    entry.Name = s_Name;
    entry.Enabled = true; // was CWorldObjectListItem(..., true, ...)
    items.push_back(entry);
    changed = true;
    s_Hint[0] = '\0';
}

void list_save(vector<CWorldObjectEntry> &items, int selected, bool &changed)
{
    const uint serial = serial_from_field();
    if (serial == 0u)
    {
        snprintf(s_Hint, sizeof(s_Hint), "Enter the object serial!");
        return;
    }
    if (selected < 0 || selected >= (int)items.size())
    {
        snprintf(s_Hint, sizeof(s_Hint), "Object is not selected!");
        return;
    }
    if (serial_in_use(items, serial, selected))
    {
        snprintf(s_Hint, sizeof(s_Hint), "Object serial is already exists (not this item)!");
        return;
    }
    items[selected].Serial = serial;
    items[selected].Name = s_Name;
    changed = true;
    s_Hint[0] = '\0';
}

void list_remove(vector<CWorldObjectEntry> &items, int &selected, bool &changed)
{
    if (selected >= 0 && selected < (int)items.size())
    {
        items.erase(items.begin() + selected);
        selected = -1;
        changed = true;
    }
}

// one accept/decline pane (the two original list groups)
void party_pane(
    const char *id,
    const char *title,
    vector<CWorldObjectEntry> &items,
    int &selected,
    int &lastSelected,
    int ftKind,
    float height)
{
    ImGui::TextUnformatted(title);
    const int buttons = ListButtons(id);
    bool changed = false;

    // the shared from-target button fills the serial/name fields (was
    // SetPartyObjectFromTarget: it did NOT add the entry to a list)
    if (ftKind >= 0 && (buttons & XALB_TARGET) != 0)
    {
        s_FtPending = true;
        g_TargetHandler = &CAssistant::HandleTargetPartyObject;
        g_Assistant.RequestTarget("Select a object for obtain serial for party");
    }

    changed |= NameList(id, items, &selected, &PartyNameOf, &PartySetNameOf, true, height);

    // selection -> fields (on_lw_PartyAuto*_clicked)
    if (selected != lastSelected)
    {
        lastSelected = selected;
        if (selected >= 0 && selected < (int)items.size())
        {
            snprintf(s_Serial, sizeof(s_Serial), "%s", CAssistant::SerialToText(items[selected].Serial).c_str());
            snprintf(s_Name, sizeof(s_Name), "%s", items[selected].Name.c_str());
        }
    }

    if ((buttons & XALB_ADD) != 0)
    {
        list_add(items, changed);
    }
    if ((buttons & XALB_SAVE) != 0)
    {
        list_save(items, selected, changed);
    }
    if ((buttons & XALB_REMOVE) != 0)
    {
        list_remove(items, selected, changed);
    }
    if (changed)
    {
        g_AssistantConfig.SaveParty(); // Party.xml (the original tab saved per mutation)
    }
}

} // namespace

void TabParty_Draw()
{
    XA_FROM_TARGET_RESULT ftResult;
    if (UI_PollFromTarget(s_FtPending, s_FtGeneration, XA_FROM_TARGET_PARTY, ftResult))
    {
        // SetPartyObjectFromTarget: serial + name when the world knows it
        snprintf(s_Serial, sizeof(s_Serial), "%s", CAssistant::SerialToText(ftResult.Serial).c_str());
        snprintf(s_Name, sizeof(s_Name), "%s", ftResult.Name.c_str());
    }

    // the four checkboxes (cb_PartyAutoAcceptAll / AutoAcceptFriendInvities /
    // AutoDeclineAll / AutoDeclineEnemyInvities; defaults per the .ui)
    bool acceptAll = g_AssistantConfig.m_PartyAutoAcceptAll;
    bool acceptFriends = g_AssistantConfig.m_PartyAutoAcceptFriends;
    bool declineAll = g_AssistantConfig.m_PartyAutoDeclineAll;
    bool declineEnemies = g_AssistantConfig.m_PartyAutoDeclineEnemies;
    bool changed = false;
    if (ImGui::Checkbox("Auto accept all invities", &acceptAll))
    {
        g_AssistantConfig.m_PartyAutoAcceptAll = acceptAll;
        changed = true;
    }
    if (ImGui::Checkbox("Auto accept invities from friends", &acceptFriends))
    {
        g_AssistantConfig.m_PartyAutoAcceptFriends = acceptFriends;
        changed = true;
    }
    if (ImGui::Checkbox("Auto decline all invities", &declineAll))
    {
        g_AssistantConfig.m_PartyAutoDeclineAll = declineAll;
        changed = true;
    }
    if (ImGui::Checkbox("Auto decline invities from enemies", &declineEnemies))
    {
        g_AssistantConfig.m_PartyAutoDeclineEnemies = declineEnemies;
        changed = true;
    }
    if (changed)
    {
        g_AssistantConfig.SaveParty();
    }

    ImGui::Separator();

    static int acceptSelected = -1;
    static int acceptLast = -1;
    static int declineSelected = -1;
    static int declineLast = -1;
    party_pane(
        "##party_accept", "List of players to auto accept:", g_AssistantConfig.m_PartyAccept,
        acceptSelected, acceptLast, XA_FROM_TARGET_PARTY, 120.0f);
    ImGui::Separator();

    // the shared Name/Serial fields between the two lists
    ImGui::TextUnformatted("Name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("##party_name", s_Name, sizeof(s_Name));
    ImGui::TextUnformatted("Serial:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("##party_serial", s_Serial, sizeof(s_Serial));
    if (s_Hint[0] != '\0')
    {
        ImGui::TextDisabled("%s", s_Hint);
    }
    ImGui::Separator();
    party_pane(
        "##party_decline", "List of players to auto decline:", g_AssistantConfig.m_PartyDecline,
        declineSelected, declineLast, -1, 120.0f);
}

void TabParty_CancelPendingFromTargets()
{
    s_FtPending = false;
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
