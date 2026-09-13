// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The six Lists sub-tabs (was assist/tablists*.ui/.cpp, §2.4): Types,
// Objects, Find, Ignore, Friends, Enemies. All bind straight to the
// g_AssistantConfig containers; every structural change persists immediately
// (the Qt tabs saved on each create/save/remove/drop and on itemDropped),
// and the save icon re-saves like the Qt save buttons did.

#include "assistantui.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include "widgets.h"
#include "../config/assistantconfig.h"
#include "../core/assist.h"
#include "../core/global.h"

namespace xuoassist {

namespace {


// NameList label accessors (plain functions keep the callbacks captureless)
static const char *TypeNameOf(const CTypeEntry &e) { return e.Name.c_str(); }
static void TypeSetNameOf(CTypeEntry &e, const char *n) { e.Name = n; }
static const char *ObjectNameOf(const CObjectEntry &e) { return e.Name.c_str(); }
static void ObjectSetNameOf(CObjectEntry &e, const char *n) { e.Name = n; }
static const char *FindNameOf(const CFindListEntry &e) { return e.Name.c_str(); }
static void FindSetNameOf(CFindListEntry &e, const char *n) { e.Name = n; }
static const char *IgnoreNameOf(const CIgnoreListEntry &e) { return e.Name.c_str(); }
static void IgnoreSetNameOf(CIgnoreListEntry &e, const char *n) { e.Name = n; }
static const char *WorldObjectNameOf(const CWorldObjectEntry &e) { return e.Name.c_str(); }
static void WorldObjectSetNameOf(CWorldObjectEntry &e, const char *n) { e.Name = n; }

const ImVec4 HINT_COLOR = ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
const ImVec4 DRESS_BAG_SET = ImVec4(0.0f, 170.0f / 255.0f, 0.0f, 1.0f);     // rgb(0,170,0)
const ImVec4 DRESS_BAG_UNSET = ImVec4(170.0f / 255.0f, 0.0f, 0.0f, 1.0f);   // rgb(170,0,0)

// case-insensitive name collision check (the Qt tabs compared toLowered texts)
bool name_in_use(const vector<CTypeEntry> &items, const char *name, int skip)
{
    for (int i = (int)items.size() - 1; i >= 0; i--)
    {
        if (i != skip && ListNamesEqual(items[i].Name, name))
        {
            return true;
        }
    }
    return false;
}

// --- Types (tabliststypes.cpp) ------------------------------------------------

void draw_types_tab()
{
    static int selected = -1;
    static int lastSelected = -1;
    static char typeName[64] = { 0 };
    static char typeGraphic[16] = { 0 };
    static char hint[128] = { 0 };

    vector<CTypeEntry> &items = g_AssistantConfig.m_Types;
    const int buttons = ListButtons("types");
    bool changed = NameList("##types", items, &selected, &TypeNameOf, &TypeSetNameOf, false, 180.0f);
    if (selected != lastSelected)
    {
        lastSelected = selected;
        hint[0] = '\0';
        if (selected >= 0 && selected < (int)items.size())
        {
            snprintf(typeName, sizeof(typeName), "%s", items[selected].Name.c_str());
            snprintf(typeGraphic, sizeof(typeGraphic), "%s", CAssistant::GraphicToText(items[selected].Type).c_str());
        }
    }

    // the static edit buffers are read directly (draw path: no scratch copies)
    if ((buttons & XALB_ADD) != 0)
    {
        // was on_pb_TypeCreate_clicked
        if (typeName[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the type name!");
        }
        else if (typeGraphic[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the type graphic!");
        }
        else if (name_in_use(items, typeName, -1))
        {
            snprintf(hint, sizeof(hint), "Type name is already exists!");
        }
        else
        {
            CTypeEntry entry;
            entry.Name = typeName;
            entry.Type = (ushort)CAssistant::TextToUInt(typeGraphic);
            items.push_back(entry);
            selected = lastSelected = (int)items.size() - 1;
            changed = true;
            hint[0] = '\0';
        }
    }
    if ((buttons & XALB_SAVE) != 0)
    {
        // was on_pb_TypeSave_clicked
        if (typeName[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the type name!");
        }
        else if (typeGraphic[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the type graphic!");
        }
        else if (selected < 0 || selected >= (int)items.size())
        {
            snprintf(hint, sizeof(hint), "Type is not selected!");
        }
        else if (name_in_use(items, typeName, selected))
        {
            snprintf(hint, sizeof(hint), "Type name is already exists (not this item)!");
        }
        else
        {
            items[selected].Name = typeName;
            items[selected].Type = (ushort)CAssistant::TextToUInt(typeGraphic);
            changed = true;
            hint[0] = '\0';
        }
    }
    if ((buttons & XALB_REMOVE) != 0 && selected >= 0 && selected < (int)items.size())
    {
        // was on_pb_TypeRemove_clicked
        items.erase(items.begin() + selected);
        selected = lastSelected = -1;
        changed = true;
    }

    if (changed)
    {
        g_AssistantConfig.SaveTypes(); // Qt saved on every mutation and drop
    }

    ImGui::TextUnformatted("Type:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    HexEditField("##type_graphic", typeGraphic, sizeof(typeGraphic));
    ImGui::TextUnformatted("Name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputText("##type_name", typeName, sizeof(typeName));
    if (hint[0] != '\0')
    {
        ImGui::TextColored(HINT_COLOR, "%s", hint);
    }
}

// --- Objects (tablistsobjects.cpp) ----------------------------------------------

void draw_objects_tab()
{
    static int selected = -1;
    static int lastSelected = -1;
    static char objectName[64] = { 0 };
    static char objectSerial[16] = { 0 };
    static char hint[128] = { 0 };

    vector<CObjectEntry> &items = g_AssistantConfig.m_Objects;
    const int buttons = ListButtons("objects");
    bool changed = NameList("##objects", items, &selected, &ObjectNameOf, &ObjectSetNameOf, false, 180.0f);
    if (selected != lastSelected)
    {
        lastSelected = selected;
        hint[0] = '\0';
        if (selected >= 0 && selected < (int)items.size())
        {
            snprintf(objectName, sizeof(objectName), "%s", items[selected].Name.c_str());
            snprintf(objectSerial, sizeof(objectSerial), "%s", CAssistant::SerialToText(items[selected].Serial).c_str());
        }
    }

    // the static edit buffers are read directly (draw path: no scratch copies)
    if ((buttons & XALB_ADD) != 0)
    {
        if (objectName[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the object name!");
        }
        else if (objectSerial[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the object serial!");
        }
        else
        {
            CObjectEntry entry;
            entry.Name = objectName;
            entry.Serial = CAssistant::TextToSerial(objectSerial);
            items.push_back(entry);
            selected = lastSelected = (int)items.size() - 1;
            changed = true;
            hint[0] = '\0';
        }
    }
    if ((buttons & XALB_SAVE) != 0)
    {
        if (objectName[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the object name!");
        }
        else if (selected < 0 || selected >= (int)items.size())
        {
            snprintf(hint, sizeof(hint), "Object is not selected!");
        }
        else
        {
            items[selected].Name = objectName;
            items[selected].Serial = CAssistant::TextToSerial(objectSerial);
            changed = true;
            hint[0] = '\0';
        }
    }
    if ((buttons & XALB_REMOVE) != 0 && selected >= 0 && selected < (int)items.size())
    {
        items.erase(items.begin() + selected);
        selected = lastSelected = -1;
        changed = true;
    }

    if (changed)
    {
        g_AssistantConfig.SaveObjects();
    }

    // Name / Serial fields
    ImGui::TextUnformatted("Name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputText("##object_name", objectName, sizeof(objectName));
    ImGui::TextUnformatted("Serial:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    HexEditField("##object_serial", objectSerial, sizeof(objectSerial));
    if (hint[0] != '\0')
    {
        ImGui::TextColored(HINT_COLOR, "%s", hint);
    }

    ImGui::Separator();

    // dress bag row (tablistsobjects.cpp:166-186, 383-421)
    if (ImGui::Button("Set as dress bag"))
    {
        const uint serial = selected >= 0 && selected < (int)items.size()
            ? items[selected].Serial
            : CAssistant::TextToSerial(objectSerial);
        if (serial == 0u)
        {
            snprintf(hint, sizeof(hint), "Select the object to set as dress bag!");
        }
        else
        {
            g_Assistant.SetDressBag(serial);
            hint[0] = '\0';
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Unset dress bag"))
    {
        g_Assistant.SetDressBag(0);
    }
    ImGui::SameLine();
    // rebuild the text only when the value changed (draw path: no per-frame
    // SerialToText allocation)
    static uint lastDressBag = 0;
    static string dressBagText = CAssistant::SerialToText(lastDressBag);
    if (g_DressBag != lastDressBag)
    {
        lastDressBag = g_DressBag;
        dressBagText = CAssistant::SerialToText(g_DressBag);
    }
    ImGui::TextColored(
        g_DressBag != 0u ? DRESS_BAG_SET : DRESS_BAG_UNSET,
        "%s",
        dressBagText.c_str());
}

// --- Find (tablistsfind.cpp, dual list) ------------------------------------------

void draw_find_tab()
{
    
    static int selectedList = -1;
    static int lastSelectedList = -1;
    static int selectedItem = -1;
    static int lastSelectedItem = -1;
    static char findName[64] = { 0 };
    static char itemGraphic[16] = { 0 };
    static char itemColor[16] = { 0 };
    static char itemComment[64] = { 0 };
    static char hint[128] = { 0 };

    vector<CFindListEntry> &lists = g_AssistantConfig.m_FindLists;

    // left: the named find lists + their fields
    const int listButtons = ListButtons("find_list");
    bool changed = NameList("##find", lists, &selectedList, &FindNameOf, &FindSetNameOf, false, 170.0f);
    if (selectedList != lastSelectedList)
    {
        lastSelectedList = selectedList;
        selectedItem = lastSelectedItem = -1;
        hint[0] = '\0';
        if (selectedList >= 0 && selectedList < (int)lists.size())
        {
            snprintf(findName, sizeof(findName), "%s", lists[selectedList].Name.c_str());
        }
    }

    // the static edit buffer is read directly (draw path: no scratch copy)
    if ((listButtons & XALB_ADD) != 0)
    {
        if (findName[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the find list name!");
        }
        else
        {
            bool exists = false;
            for (int i = (int)lists.size() - 1; i >= 0; i--)
            {
                if (ListNamesEqual(lists[i].Name, findName))
                {
                    exists = true;
                    break;
                }
            }
            if (exists)
            {
                snprintf(hint, sizeof(hint), "Find list name is already exists!");
            }
            else
            {
                CFindListEntry entry;
                entry.Name = findName;
                lists.push_back(entry);
                selectedList = lastSelectedList = (int)lists.size() - 1;
                changed = true;
                hint[0] = '\0';
            }
        }
    }
    if ((listButtons & XALB_SAVE) != 0)
    {
        if (findName[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the find list name!");
        }
        else if (selectedList < 0 || selectedList >= (int)lists.size())
        {
            snprintf(hint, sizeof(hint), "List is not selected!");
        }
        else
        {
            lists[selectedList].Name = findName;
            changed = true;
            hint[0] = '\0';
        }
    }
    if ((listButtons & XALB_REMOVE) != 0 && selectedList >= 0 && selectedList < (int)lists.size())
    {
        lists.erase(lists.begin() + selectedList);
        selectedList = lastSelectedList = -1;
        selectedItem = lastSelectedItem = -1;
        changed = true;
    }

    if (changed)
    {
        g_AssistantConfig.SaveFindList();
    }

    ImGui::TextUnformatted("Find list name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("##find_name", findName, sizeof(findName));
    if (hint[0] != '\0')
    {
        ImGui::TextColored(HINT_COLOR, "%s", hint);
    }

    ImGui::SameLine();

    // right: the selected list's content (Graphic/Color/Comment rows)
    ImGui::BeginGroup();
    ImGui::TextUnformatted("Selected find list content:");
    if (selectedList < 0 || selectedList >= (int)lists.size())
    {
        ImGui::TextDisabled("(select a find list)");
    }
    else
    {
        vector<CFindItem> &content = lists[selectedList].m_Items;
        const int itemButtons = ListButtons("find_content");
        bool contentChanged = FindContentList("##find_content", content, &selectedItem, 170.0f);
        if (selectedItem != lastSelectedItem)
        {
            lastSelectedItem = selectedItem;
            if (selectedItem >= 0 && selectedItem < (int)content.size())
            {
                snprintf(itemGraphic, sizeof(itemGraphic), "%s", content[selectedItem].GetGraphic().c_str());
                snprintf(itemColor, sizeof(itemColor), "%s", content[selectedItem].GetColor().c_str());
                snprintf(itemComment, sizeof(itemComment), "%s", content[selectedItem].GetComment().c_str());
            }
        }

        // the static edit buffers are read directly (draw path: no scratch copies)
        if ((itemButtons & XALB_ADD) != 0)
        {
            if (itemGraphic[0] == '\0')
            {
                snprintf(hint, sizeof(hint), "Enter the content graphic!");
            }
            else
            {
                CFindItem item(itemGraphic, itemColor);
                item.SetComment(itemComment);
                content.push_back(item);
                selectedItem = lastSelectedItem = (int)content.size() - 1;
                contentChanged = true;
                hint[0] = '\0';
            }
        }
        if ((itemButtons & XALB_SAVE) != 0 && selectedItem >= 0 && selectedItem < (int)content.size())
        {
            content[selectedItem].SetGraphic(itemGraphic);
            content[selectedItem].SetColor(itemColor);
            content[selectedItem].SetComment(itemComment);
            contentChanged = true;
            hint[0] = '\0';
        }
        if ((itemButtons & XALB_REMOVE) != 0 && selectedItem >= 0 && selectedItem < (int)content.size())
        {
            content.erase(content.begin() + selectedItem);
            selectedItem = lastSelectedItem = -1;
            contentChanged = true;
        }
        if (contentChanged)
        {
            g_AssistantConfig.SaveFindList();
        }

        ImGui::TextUnformatted("Graphic:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        HexEditField("##find_graphic", itemGraphic, sizeof(itemGraphic));
        ImGui::TextUnformatted("Color:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        HexEditField("##find_color", itemColor, sizeof(itemColor));
        ImGui::TextUnformatted("Comment:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputText("##find_comment", itemComment, sizeof(itemComment));
    }
    ImGui::EndGroup();
}

// --- Ignore (tablistsignore.cpp, dual list) ----------------------------------------

void draw_ignore_tab()
{
    
    static int selectedList = -1;
    static int lastSelectedList = -1;
    static int selectedItem = -1;
    static int lastSelectedItem = -1;
    static char ignoreName[64] = { 0 };
    static char itemSerial[16] = { 0 };
    static char itemGraphic[16] = { 0 };
    static char itemColor[16] = { 0 };
    static char itemComment[64] = { 0 };
    static char hint[128] = { 0 };

    vector<CIgnoreListEntry> &lists = g_AssistantConfig.m_IgnoreLists;

    const int listButtons = ListButtons("ignore_list");
    bool changed = NameList("##ignore", lists, &selectedList, &IgnoreNameOf, &IgnoreSetNameOf, false, 170.0f);
    if (selectedList != lastSelectedList)
    {
        lastSelectedList = selectedList;
        selectedItem = lastSelectedItem = -1;
        hint[0] = '\0';
        if (selectedList >= 0 && selectedList < (int)lists.size())
        {
            snprintf(ignoreName, sizeof(ignoreName), "%s", lists[selectedList].Name.c_str());
        }
    }

    // the static edit buffers are read directly (draw path: no scratch copies)
    if ((listButtons & XALB_ADD) != 0)
    {
        if (ignoreName[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the ignore list name!");
        }
        else
        {
            bool exists = false;
            for (int i = (int)lists.size() - 1; i >= 0; i--)
            {
                if (ListNamesEqual(lists[i].Name, ignoreName))
                {
                    exists = true;
                    break;
                }
            }
            if (exists)
            {
                snprintf(hint, sizeof(hint), "Ignore list name is already exists!");
            }
            else
            {
                CIgnoreListEntry entry;
                entry.Name = ignoreName;
                lists.push_back(entry);
                selectedList = lastSelectedList = (int)lists.size() - 1;
                changed = true;
                hint[0] = '\0';
            }
        }
    }
    if ((listButtons & XALB_SAVE) != 0)
    {
        if (ignoreName[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the ignore list name!");
        }
        else if (selectedList < 0 || selectedList >= (int)lists.size())
        {
            snprintf(hint, sizeof(hint), "List is not selected!");
        }
        else
        {
            lists[selectedList].Name = ignoreName;
            changed = true;
            hint[0] = '\0';
        }
    }
    if ((listButtons & XALB_REMOVE) != 0 && selectedList >= 0 && selectedList < (int)lists.size())
    {
        lists.erase(lists.begin() + selectedList);
        selectedList = lastSelectedList = -1;
        selectedItem = lastSelectedItem = -1;
        changed = true;
    }

    if (changed)
    {
        g_AssistantConfig.SaveIgnoreList();
    }

    ImGui::TextUnformatted("Ignore list name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("##ignore_name", ignoreName, sizeof(ignoreName));
    if (hint[0] != '\0')
    {
        ImGui::TextColored(HINT_COLOR, "%s", hint);
    }

    ImGui::SameLine();

    ImGui::BeginGroup();
    ImGui::TextUnformatted("Selected ignore list content:");
    if (selectedList < 0 || selectedList >= (int)lists.size())
    {
        ImGui::TextDisabled("(select an ignore list)");
    }
    else
    {
        vector<CIgnoreItem> &content = lists[selectedList].m_Items;
        const int itemButtons = ListButtons("ignore_content");
        bool contentChanged = IgnoreContentList("##ignore_content", content, &selectedItem, 170.0f);
        if (selectedItem != lastSelectedItem)
        {
            lastSelectedItem = selectedItem;
            if (selectedItem >= 0 && selectedItem < (int)content.size())
            {
                snprintf(itemSerial, sizeof(itemSerial), "%s", content[selectedItem].GetSerial().c_str());
                snprintf(itemGraphic, sizeof(itemGraphic), "%s", content[selectedItem].GetGraphic().c_str());
                snprintf(itemColor, sizeof(itemColor), "%s", content[selectedItem].GetColor().c_str());
                snprintf(itemComment, sizeof(itemComment), "%s", content[selectedItem].GetComment().c_str());
            }
        }

        // the static edit buffers are read directly (draw path: no scratch copies)
        if ((itemButtons & XALB_ADD) != 0)
        {
            if (itemSerial[0] == '\0')
            {
                snprintf(hint, sizeof(hint), "Enter the content serial!");
            }
            else
            {
                CIgnoreItem item(itemSerial, itemGraphic, itemColor);
                item.SetComment(itemComment);
                content.push_back(item);
                selectedItem = lastSelectedItem = (int)content.size() - 1;
                contentChanged = true;
                hint[0] = '\0';
            }
        }
        if ((itemButtons & XALB_SAVE) != 0 && selectedItem >= 0 && selectedItem < (int)content.size())
        {
            content[selectedItem].SetSerial(itemSerial);
            content[selectedItem].SetGraphic(itemGraphic);
            content[selectedItem].SetColor(itemColor);
            content[selectedItem].SetComment(itemComment);
            contentChanged = true;
            hint[0] = '\0';
        }
        if ((itemButtons & XALB_REMOVE) != 0 && selectedItem >= 0 && selectedItem < (int)content.size())
        {
            content.erase(content.begin() + selectedItem);
            selectedItem = lastSelectedItem = -1;
            contentChanged = true;
        }
        if (contentChanged)
        {
            g_AssistantConfig.SaveIgnoreList();
        }

        ImGui::TextUnformatted("Serial:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        HexEditField("##ignore_serial", itemSerial, sizeof(itemSerial));
        ImGui::TextUnformatted("Graphic:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        HexEditField("##ignore_graphic", itemGraphic, sizeof(itemGraphic));
        ImGui::TextUnformatted("Color:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        HexEditField("##ignore_color", itemColor, sizeof(itemColor));
        ImGui::TextUnformatted("Comment:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputText("##ignore_comment", itemComment, sizeof(itemComment));
    }
    ImGui::EndGroup();
}

// --- Friends / Enemies (tablistsfriends.cpp / tablistsenemies.cpp) ------------------

void draw_world_object_tab(
    const char *listId,
    vector<CWorldObjectEntry> &items,
    bool checkColumn,
    int *selected,
    int *lastSelected,
    char *name,
    size_t nameSize,
    char *serial,
    size_t serialSize,
    char *hint,
    bool &changed)
{
    const int buttons = ListButtons(listId);
    changed = NameList(listId, items, selected, &WorldObjectNameOf, &WorldObjectSetNameOf, checkColumn, 180.0f);
    if (*selected != *lastSelected)
    {
        *lastSelected = *selected;
        hint[0] = '\0';
        if (*selected >= 0 && *selected < (int)items.size())
        {
            snprintf(name, nameSize, "%s", items[*selected].Name.c_str());
            snprintf(serial, serialSize, "%s", CAssistant::SerialToText(items[*selected].Serial).c_str());
        }
    }

    // the edit buffers are read directly (draw path: no scratch copies)
    if ((buttons & XALB_ADD) != 0)
    {
        if (name[0] == '\0')
        {
            snprintf(hint, 128, "Enter the name!");
        }
        else if (serial[0] == '\0')
        {
            snprintf(hint, 128, "Enter the serial!");
        }
        else
        {
            CWorldObjectEntry entry;
            entry.Name = name;
            entry.Serial = CAssistant::TextToSerial(serial);
            entry.Enabled = true;
            items.push_back(entry);
            *selected = *lastSelected = (int)items.size() - 1;
            changed = true;
            hint[0] = '\0';
        }
    }
    if ((buttons & XALB_SAVE) != 0)
    {
        if (name[0] == '\0')
        {
            snprintf(hint, 128, "Enter the name!");
        }
        else if (*selected < 0 || *selected >= (int)items.size())
        {
            snprintf(hint, 128, "Object is not selected!");
        }
        else
        {
            items[*selected].Name = name;
            items[*selected].Serial = CAssistant::TextToSerial(serial);
            changed = true;
            hint[0] = '\0';
        }
    }
    if ((buttons & XALB_REMOVE) != 0 && *selected >= 0 && *selected < (int)items.size())
    {
        items.erase(items.begin() + *selected);
        *selected = *lastSelected = -1;
        changed = true;
    }

    ImGui::TextUnformatted("Name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("##wo_name", name, nameSize);
    ImGui::TextUnformatted("Serial:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    HexEditField("##wo_serial", serial, serialSize);
    if (hint[0] != '\0')
    {
        ImGui::TextColored(HINT_COLOR, "%s", hint);
    }
}

void draw_friends_tab()
{
    static int selected = -1;
    static int lastSelected = -1;
    static char name[64] = { 0 };
    static char serial[16] = { 0 };
    static char hint[128] = { 0 };

    bool changed = false;
    draw_world_object_tab(
        "##friends", g_AssistantConfig.m_Friends, true, &selected, &lastSelected,
        name, sizeof(name), serial, sizeof(serial), hint, changed);
    if (changed)
    {
        g_AssistantConfig.SaveFriends();
    }
    if (ImGui::Checkbox("Party members as friends", &g_AssistantConfig.m_FeelPartyMembersAsFriends))
    {
        g_AssistantConfig.SaveFriends(); // the attribute rides Friends.xml
    }
}

void draw_enemies_tab()
{
    static int selected = -1;
    static int lastSelected = -1;
    static char name[64] = { 0 };
    static char serial[16] = { 0 };
    static char hint[128] = { 0 };

    bool changed = false;
    draw_world_object_tab(
        "##enemies", g_AssistantConfig.m_Enemies, false, &selected, &lastSelected,
        name, sizeof(name), serial, sizeof(serial), hint, changed);
    if (changed)
    {
        g_AssistantConfig.SaveEnemies();
    }
}

} // namespace

void TabLists_Draw(int subTab)
{
    switch (subTab)
    {
        case TAB_LISTS_TYPES:
            draw_types_tab();
            break;
        case TAB_LISTS_OBJECTS:
            draw_objects_tab();
            break;
        case TAB_LISTS_FIND:
            draw_find_tab();
            break;
        case TAB_LISTS_IGNORE:
            draw_ignore_tab();
            break;
        case TAB_LISTS_FRIENDS:
            draw_friends_tab();
            break;
        case TAB_LISTS_ENEMIES:
            draw_enemies_tab();
            break;
        default:
            break;
    }
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
