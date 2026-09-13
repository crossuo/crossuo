// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The Object Inspector window (see inspector.h). Snapshots rebuild from the
// mirror on open / OnInspectorUpdate / a 2 Hz refresh while visible - never
// per frame (the rebuild formats ~40 strings into the section storage).

#define LOGGER_MODULE Assistant

#include "inspector.h"

#if defined(XUO_ASSISTANT)

#include <common/logging/logging.h>

#include <external/gfx/imgui/imgui.h>

#include "../core/global.h"
#include "../core/textconv.h"
#include "../model/gameobjects/gamecharacter.h"
#include "../model/gameobjects/gameitem.h"
#include "../model/gameobjects/gameplayer.h"
#include "../model/gameobjects/gameworld.h"

namespace xuoassist {

namespace {

// the tree row keys (the original OIK_*; identity is all the port needs)
enum OIK
{
    OIK_GROUP = 0,
    OIK_SERIAL,
    OIK_CONTAINER,
    OIK_TOP_OBJECT,
    OIK_NAME,
    OIK_GRAPHIC,
    OIK_COLOR,
    OIK_COUNT,
    OIK_X,
    OIK_Y,
    OIK_Z,
    OIK_MAP,
    OIK_FLAGS,
    OIK_IGNORED,
    OIK_LAYER,
    OIK_HITS,
    OIK_NOTORIETY,
    OIK_STATS,
    OIK_RESISTS
};

struct XA_INSPECT_ROW
{
    int Depth{ 0 };
    bool Group{ false };
    const char *Key{ "" };   // static text
    string Value;            // the copied-on-click payload
    int RowKey{ OIK_GROUP };
};

struct XA_INSPECTOR_SECTION
{
    uint Serial{ 0 };
    bool OutOfRange{ false };
    vector<XA_INSPECT_ROW> Rows;
    uint32_t LastBuildTick{ 0 };
};

vector<XA_INSPECTOR_SECTION> s_Sections;
bool s_WindowOpen = false;
uint s_FocusSerial = 0; // scroll-to request from Inspector_Open

XA_INSPECTOR_SECTION *FindSection(uint serial)
{
    for (XA_INSPECTOR_SECTION &section : s_Sections)
    {
        if (section.Serial == serial)
        {
            return &section;
        }
    }
    return nullptr;
}

void AddRow(
    vector<XA_INSPECT_ROW> &rows,
    int depth,
    bool group,
    const char *key,
    const string &value,
    int rowKey)
{
    XA_INSPECT_ROW row;
    row.Depth = depth;
    row.Group = group;
    row.Key = key;
    row.Value = value;
    row.RowKey = rowKey;
    rows.push_back(row);
}

// TabObjectInspector::Update body, tree rows only (art preview dropped)
void BuildRows(vector<XA_INSPECT_ROW> &rows, CGameObject *obj)
{
    rows.clear();

    AddRow(rows, 0, true, "Game Object", "", OIK_GROUP);
    AddRow(rows, 1, false, "Serial: ", SerialToText(obj->GetSerial()), OIK_SERIAL);
    AddRow(rows, 1, false, "Container: ", SerialToText(obj->GetContainer()), OIK_CONTAINER);

    CGameObject *topObject = obj->GetTopObject();
    const uint topObjectSerial =
        ((topObject != nullptr && topObject != obj) ? topObject->GetSerial() : 0xFFFFFFFF);
    AddRow(rows, 1, false, "Top Object: ", SerialToText(topObjectSerial), OIK_TOP_OBJECT);

    AddRow(rows, 1, false, "Name: ", obj->GetName(), OIK_NAME);
    AddRow(rows, 1, false, "Graphic: ", GraphicToText(obj->GetGraphic()), OIK_GRAPHIC);
    AddRow(rows, 1, false, "Color: ", GraphicToText(obj->GetColor()), OIK_COLOR);

    char num[32] = { 0 };

    snprintf(num, sizeof(num), "%u", obj->GetCount());
    AddRow(rows, 1, false, "Count: ", num, OIK_COUNT);
    snprintf(num, sizeof(num), "%d", obj->GetX());
    AddRow(rows, 1, false, "X: ", num, OIK_X);
    snprintf(num, sizeof(num), "%d", obj->GetY());
    AddRow(rows, 1, false, "Y: ", num, OIK_Y);
    snprintf(num, sizeof(num), "%d", obj->GetZ());
    AddRow(rows, 1, false, "Z: ", num, OIK_Z);
    snprintf(num, sizeof(num), "%d", (int)obj->GetMapIndex());
    AddRow(rows, 1, false, "Map: ", num, OIK_MAP);

    // the flags subgroup
    snprintf(num, sizeof(num), "0x%x", obj->GetFlags());
    AddRow(rows, 1, true, "Flags: ", num, OIK_FLAGS);
    AddRow(rows, 2, false, "Frozen: ", obj->Frozen() ? "True" : "False", OIK_FLAGS);
    AddRow(rows, 2, false, "Poisoned: ", obj->Poisoned() ? "True" : "False", OIK_FLAGS);
    AddRow(rows, 2, false, "Yellow Hits: ", obj->YellowHits() ? "True" : "False", OIK_FLAGS);
    AddRow(rows, 2, false, "Locked: ", obj->Locked() ? "True" : "False", OIK_FLAGS);
    AddRow(rows, 2, false, "In War Mode: ", obj->InWarMode() ? "True" : "False", OIK_FLAGS);
    AddRow(rows, 2, false, "Hidden: ", obj->Hidden() ? "True" : "False", OIK_FLAGS);
    AddRow(rows, 2, false, "Flying: ", obj->Flying() ? "True" : "False", OIK_FLAGS);

    AddRow(rows, 1, false, "Ignored: ", obj->GetIgnored() ? "True" : "False", OIK_IGNORED);

    if (!obj->GetNPC())
    {
        CGameItem *item = (CGameItem *)obj;
        string title = "Game Item";
        if (obj->IsCorpse())
        {
            title += " (Corpse)";
        }
        AddRow(rows, 0, true, title.c_str(), "", OIK_GROUP);
        const string layer = g_LayerName[item->GetLayer() % 30] + " (" +
                             std::to_string((int)item->GetLayer()) + ")";
        AddRow(rows, 1, false, "Layer: ", layer, OIK_LAYER);
    }
    else
    {
        CGameCharacter *character = (CGameCharacter *)obj;
        string title = "Game Character";
        if (obj->IsHuman())
        {
            title += " (Humanoid)";
        }
        AddRow(rows, 0, true, title.c_str(), "", OIK_GROUP);

        auto snum = [&num](int value)
        {
            snprintf(num, sizeof(num), "%d", value);
            return (const char *)num;
        };
        AddRow(rows, 1, false, "Hits: ", snum(character->GetHits()), OIK_HITS);
        AddRow(rows, 1, false, "Max Hits: ", snum(character->GetMaxHits()), OIK_HITS);
        AddRow(rows, 1, false, "Mana: ", snum(character->GetMana()), OIK_HITS);
        AddRow(rows, 1, false, "Max Mana: ", snum(character->GetMaxMana()), OIK_HITS);
        AddRow(rows, 1, false, "Stam: ", snum(character->GetStam()), OIK_HITS);
        AddRow(rows, 1, false, "Max Stam: ", snum(character->GetMaxStam()), OIK_HITS);
        AddRow(rows, 1, false, "Sex: ", character->GetSex() ? "Man" : "Woman", OIK_STATS);
        static const char *const raceNames[3] = { "Human", "Elf", "Gargoyle" };
        AddRow(
            rows,
            1,
            false,
            "Race: ",
            raceNames[((int)character->GetRace() - 1) % 3],
            OIK_STATS);
        AddRow(rows, 1, false, "Direction: ", snum(character->GetDirection()), OIK_STATS);
        AddRow(
            rows,
            1,
            false,
            "Running: ",
            ((character->GetDirection() & 0x80) != 0) ? "True" : "False",
            OIK_STATS);
        AddRow(rows, 1, false, "Notoriety: ", snum(character->GetNotoriety()), OIK_NOTORIETY);
        AddRow(
            rows,
            1,
            false,
            "Can Change Name: ",
            character->GetCanChangeName() ? "True" : "False",
            OIK_STATS);

        if (obj->IsPlayer())
        {
            CPlayer *player = (CPlayer *)obj;
            AddRow(rows, 0, true, "Game Player", "", OIK_GROUP);
            AddRow(rows, 1, false, "Str: ", snum(player->GetStr()), OIK_STATS);
            AddRow(rows, 1, false, "Int: ", snum(player->GetInt()), OIK_STATS);
            AddRow(rows, 1, false, "Dex: ", snum(player->GetDex()), OIK_STATS);
            AddRow(rows, 1, false, "Stats Cap: ", snum(player->GetStatsCap()), OIK_STATS);
            static const char *const lockNames[3] = { "Up", "Down", "Lock" };
            AddRow(rows, 1, false, "Lock State Str: ", lockNames[player->GetLockStr() % 3], OIK_STATS);
            AddRow(rows, 1, false, "Lock State Int: ", lockNames[player->GetLockInt() % 3], OIK_STATS);
            AddRow(rows, 1, false, "Lock State Dex: ", lockNames[player->GetLockDex() % 3], OIK_STATS);
            AddRow(rows, 1, false, "Weight: ", snum(player->GetWeight()), OIK_STATS);
            AddRow(rows, 1, false, "Max Weight: ", snum(player->GetMaxWeight()), OIK_STATS);
            AddRow(rows, 1, false, "Armor: ", snum(player->GetArmor()), OIK_RESISTS);
            AddRow(rows, 1, false, "Gold: ", snum((int)player->GetGold()), OIK_STATS);
            AddRow(rows, 1, false, "Warmode: ", player->GetWarmode() ? "True" : "False", OIK_STATS);
            AddRow(rows, 1, false, "Followers: ", snum(player->GetFollowers()), OIK_STATS);
            AddRow(rows, 1, false, "Max Followers: ", snum(player->GetMaxFollowers()), OIK_STATS);
            AddRow(rows, 1, false, "Fire Resistance: ", snum(player->GetFireResistance()), OIK_RESISTS);
            AddRow(rows, 1, false, "Cold Resistance: ", snum(player->GetColdResistance()), OIK_RESISTS);
            AddRow(
                rows, 1, false, "Poison Resistance: ", snum(player->GetPoisonResistance()), OIK_RESISTS);
            AddRow(
                rows,
                1,
                false,
                "Energy Resistance: ",
                snum(player->GetEnergyResistance()),
                OIK_RESISTS);
            AddRow(rows, 1, false, "Luck: ", snum(player->GetLuck()), OIK_RESISTS);
            AddRow(rows, 1, false, "Min Damage: ", snum(player->GetMinDamage()), OIK_RESISTS);
            AddRow(rows, 1, false, "Max Damage: ", snum(player->GetMaxDamage()), OIK_RESISTS);
            AddRow(rows, 1, false, "Tithing Points: ", snum((int)player->GetTithingPoints()), OIK_STATS);
            AddRow(rows, 1, false, "Stealth Steps: ", snum(player->GetStealthSteps()), OIK_STATS);
        }
    }
}

void RebuildSection(XA_INSPECTOR_SECTION &section)
{
    section.OutOfRange = true;
    if (g_World != nullptr)
    {
        // The original: CheckUpdate found the tab by serial, then read the object - a
        // serial no longer in the mirror just kept the previous snapshot
        CGameObject *obj = g_World->FindWorldObject(section.Serial);
        if (obj != nullptr)
        {
            section.OutOfRange = false;
            BuildRows(section.Rows, obj);
        }
    }
    section.LastBuildTick = g_Ticks;
}

} // namespace

void Inspector_Open(uint serial)
{
    if (serial == 0u)
    {
        return;
    }

    s_WindowOpen = true;
    s_FocusSerial = serial;

    XA_INSPECTOR_SECTION *section = FindSection(serial);
    if (section == nullptr)
    {
        XA_INSPECTOR_SECTION fresh;
        fresh.Serial = serial;
        s_Sections.push_back(fresh);
        section = &s_Sections.back();
    }

    RebuildSection(*section);
}

void Inspector_OnInspectorUpdate(uint serial)
{
    XA_INSPECTOR_SECTION *section = FindSection(serial);
    if (section != nullptr)
    {
        RebuildSection(*section);
    }
}

void Inspector_Shutdown()
{
    s_Sections.clear();
    s_WindowOpen = false;
    s_FocusSerial = 0;
}

void InspectorWindow_Draw()
{
    if (!s_WindowOpen)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(426.0f, 549.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Object Inspector", &s_WindowOpen))
    {
        ImGui::End();
        return;
    }

    // the column caption (objectinspectorform.ui: single tree column)
    ImGui::TextDisabled("Object Information (click on field for copy value)");
    ImGui::Separator();

    // 2 Hz refresh of the visible sections (was the frequent CheckUpdate
    // event stream; the snapshot semantics stay the same)
    static uint32_t s_LastRefresh = 0;
    if (g_Ticks - s_LastRefresh >= 500u)
    {
        s_LastRefresh = g_Ticks;
        for (XA_INSPECTOR_SECTION &section : s_Sections)
        {
            if (!section.OutOfRange)
            {
                RebuildSection(section);
            }
        }
    }

    // one collapsible section per inspected serial (original: one tab per serial)
    for (XA_INSPECTOR_SECTION &section : s_Sections)
    {
        char header[64] = { 0 };
        snprintf(
            header,
            sizeof(header),
            "%s%s",
            SerialToText(section.Serial).c_str(),
            section.OutOfRange ? " (out of range)" : "");
        const bool open =
            ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen);
        if (s_FocusSerial == section.Serial && open)
        {
            ImGui::SetScrollHereY(0.0f);
            s_FocusSerial = 0;
        }
        if (!open)
        {
            continue;
        }

        ImGui::Indent();
        for (const XA_INSPECT_ROW &row : section.Rows)
        {
            if (row.Group)
            {
                // group rows fold like the original tree nodes
                char groupId[128];
                snprintf(groupId, sizeof(groupId), "%s##%u_%d_%d", row.Key, section.Serial,
                         row.Depth, (int)section.Rows.size());
                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                ImGui::CollapsingHeader(groupId);
                continue;
            }

            ImGui::Indent(row.Depth > 1 ? 16.0f * (row.Depth - 1) : 0.0f);
            ImGui::TextUnformatted(row.Key);
            ImGui::SameLine();
            ImGui::TextUnformatted(row.Value.c_str());
            ImGui::Unindent(row.Depth > 1 ? 16.0f * (row.Depth - 1) : 0.0f);

            // leaf click copies the value
            if (ImGui::IsItemClicked())
            {
                ImGui::SetClipboardText(row.Value.c_str());
            }
        }
        ImGui::Unindent();
    }

    ImGui::End();
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
