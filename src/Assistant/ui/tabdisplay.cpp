// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// TabDisplay (was assist/tabdisplay.ui/.cpp, §2.3): the three nested pages -
// Configuration (display mode radios, flags, colors, title data + the insert
// command / set title from group popups), Items editor (the custom display
// items with their color/value tables) and Group editor. The data lives in
// g_AssistantConfig (DisplayItems.xml/DisplayGroups.xml/Options.xml display
// section); the compiled title output goes to the OS window caption through
// the manager's Redraw + AssistantUI_Tick (was the win32 GDI title bar).
//
// Port deltas (documented): the color swatches pick opaque RGB through a
// picker popup; the value/color tables render as plain text next to the
// pickers instead of label grids; there is no drag-reorder list here
// (the original lw_DisplayItems was a plain list - no itemDropped connect beyond
// the Options.xml save, which the mutations do directly); the Insert command
// / Set title from group actions APPEND the tag at the end of s_TitleData,
// while the original inserted at the text cursor - inserting at the InputText cursor
// is possible through the ImGui input-state accessors (documented only,
// deliberately not implemented).

#include "assistantui.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>
#include <external/gfx/imgui/imgui_internal.h> // RegisterUserTexture (icon previews)

#include "widgets.h"
#include "assistanteventsui.h"
#include "../config/assistantconfig.h"
#include "../core/assist.h"
#include "../core/global.h"
#include "../managers/displaymanager.h"
#include "../model/commonitems/displayitem.h"
#include <map>

namespace xuoassist {

namespace {

// --- icon previews (the original items editor showed the decoded static art in
// --- the list rows; the ImGui list core is text-only, so the preview lives
// --- next to the selected item's properties) -------------------------------

// the ImGui upload of a decoded CDisplayIcon, cached per (graphic<<16)|hue,
// LRU-capped (a long session can preview many graphic+hue combos; without
// the cap the CPU-side pixel buffers accumulate for the process lifetime)
static constexpr int MAX_ICON_TEXTURES = 64;
struct CachedIconTexture
{
    ImTextureData *tex;
    int lastUseFrame; // ImGui::GetFrameCount() at the last cache hit / insert
};
static std::map<uint, CachedIconTexture> s_IconTextures;
// evicted entries: queued for the backend destroy, unregistered + freed once
// it reports Destroyed (they cannot be deleted early: they still sit in the
// context's UserTextures list)
vector<ImTextureData *> s_IconRetired;

// the project headers above pull in mappedfile.h -> common/logging ->
// SDL_wrapper -> SDL_syswm.h -> X11/Xlib.h, whose `Status` macro collides
// with the ImTextureStatus members below; Xlib is fully processed by now
#ifdef Status
#undef Status
#endif

static void IconReapRetired()
{
    for (size_t i = 0; i < s_IconRetired.size();)
    {
        ImTextureData *tex = s_IconRetired[i];
        if (tex->Status == ImTextureStatus_Destroyed)
        {
            ImGui::UnregisterUserTexture(tex);
            delete tex;
            s_IconRetired.erase(s_IconRetired.begin() + i);
        }
        else
        {
            i++;
        }
    }
}

static ImTextureData *IconTextureFor(CDisplayIcon *icon, uint key)
{
    if (icon == nullptr || icon->Width <= 0 || icon->Height <= 0 ||
        icon->Pixels.empty() || ImGui::GetCurrentContext() == nullptr)
    {
        return nullptr;
    }

    IconReapRetired();

    const int frame = ImGui::GetFrameCount();
    auto i = s_IconTextures.find(key);
    if (i != s_IconTextures.end())
    {
        i->second.lastUseFrame = frame;
        return i->second.tex;
    }

    if ((int)s_IconTextures.size() >= MAX_ICON_TEXTURES)
    {
        // evict the least recently used entry: queue the backend destroy and
        // move it to the retire list (the canonical sequence of the map resize
        // retirements in AssMap_RadarTexture)
        auto oldest = s_IconTextures.begin();
        for (auto it = s_IconTextures.begin(); it != s_IconTextures.end(); ++it)
        {
            if (it->second.lastUseFrame < oldest->second.lastUseFrame)
            {
                oldest = it;
            }
        }
        oldest->second.tex->WantDestroyNextFrame = true;
        s_IconRetired.push_back(oldest->second.tex);
        s_IconTextures.erase(oldest);
    }

    ImTextureData *tex = new ImTextureData();
    tex->Create(ImTextureFormat_RGBA32, icon->Width, icon->Height);
    tex->SetStatus(ImTextureStatus_WantCreate);
    tex->UseColors = true;
    memcpy(tex->GetPixels(), icon->Pixels.data(), (size_t)tex->GetSizeInBytes());
    ImGui::RegisterUserTexture(tex);
    s_IconTextures[key] = { tex, frame };
    return tex;
}

// --- page state (the original widget fields) ---------------------------------
char s_TitleData[2048] = { 0 };   // te_DisplayTitleData
string s_CompiledFrom;            // the data the manager last compiled (text|color)
bool s_TitleDirty = false;        // a redraw was requested (events / edits)

int s_SelectedItem = -1;          // lw_DisplayItems
int s_LastSelectedItem = -2;
char s_ItemName[64] = { 0 };      // le_DisplayItemName
char s_ItemGraphic[16] = { 0 };   // le_DisplayItemGraphic
char s_ItemHue[16] = { 0 };       // le_DisplayItemHue
char s_ItemText[128] = { 0 };     // le_DisplayItemText
int s_ItemMinValue = 0;           // sb_DisplayItemMinValue
int s_ItemMidValue = 0;           // sb_DisplayItemMidValue
int s_ItemTypeIndex = 0;          // cb_DisplayItemType
char s_Hint[128] = { 0 };

int s_SelectedGroup = -1;         // lw_DisplayGroups
char s_GroupName[64] = { 0 };     // le_DisplayGroupName
char s_GroupData[2048] = { 0 };   // te_DisplayGroupTitleData

bool s_FtPending = false;         // the items editor from-target
uint s_FtGeneration = 0;

// 0xFFRRGGBB (what the original writers stored) <-> ImGui IM_COL32 RGBA
ImU32 argb_to_rgba(uint argb)
{
    return IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, 0xFF);
}

uint rgba_to_argb(ImU32 rgba)
{
    const ImVec4 f = ImGui::ColorConvertU32ToFloat4(rgba);
    const uint r = (uint)(f.x * 255.0f + 0.5f);
    const uint g = (uint)(f.y * 255.0f + 0.5f);
    const uint b = (uint)(f.z * 255.0f + 0.5f);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

// ColorSwatch: swatch + picker bound to a stored RGBA value. The value is
// written through on every frame of a drag (the live preview); true is
// returned only on the commit frame (picker deactivated after an edit) so
// the callers throttle their disk saves to the end of the session.
bool color_field(const char *id, uint *argb)
{
    ImU32 rgba = argb_to_rgba(*argb);
    bool commit = false;
    ColorSwatch(id, &rgba, 0.0f, &commit);
    *argb = rgba_to_argb(rgba);
    return commit;
}

const vector<CDisplayInfo> &commands()
{
    return g_DisplayManager.GetCommands();
}

// the usable rows of cb_DisplayItemType (Name is empty for separators)
int command_count()
{
    int count = 0;
    for (size_t i = 0; i < commands().size(); i++)
    {
        if (commands()[i].Name.length() != 0)
        {
            count++;
        }
    }
    return count;
}

const CDisplayInfo &command_at(int index)
{
    static CDisplayInfo empty;
    int count = 0;
    for (size_t i = 0; i < commands().size(); i++)
    {
        if (commands()[i].Name.length() != 0)
        {
            if (count == index)
            {
                return commands()[i];
            }
            count++;
        }
    }
    return empty;
}

// was ChangeDisplayItemContent: the selected item -> the editor fields
void item_to_fields(const CDisplayItem &item, bool fullUpdate)
{
    if (fullUpdate)
    {
        s_ItemTypeIndex = 0;
        for (size_t i = 0; i < commands().size(); i++)
        {
            if (commands()[i].Type == item.GetType() && commands()[i].Name.length() != 0)
            {
                const int count = command_count();
                for (int j = 0; j < count; j++)
                {
                    if (command_at(j).Name == commands()[i].Name)
                    {
                        s_ItemTypeIndex = j;
                        break;
                    }
                }
                break;
            }
        }
    }

    snprintf(s_ItemText, sizeof(s_ItemText), "%s", item.GetText().c_str());
    snprintf(s_ItemGraphic, sizeof(s_ItemGraphic), "%s", CAssistant::GraphicToText(item.GetGraphic()).c_str());
    snprintf(s_ItemHue, sizeof(s_ItemHue), "%s", CAssistant::GraphicToText(item.GetHue()).c_str());
    s_ItemMidValue = item.GetMidValue();
    s_ItemMinValue = item.GetMinValue();
}

// was SaveDisplayItemData: the editor fields -> the entry (+ save + recompile).
// The colors/background/percents are NOT copied here: the pickers and checks
// write the entry directly (see draw_items_editor_page).
void fields_to_item(CDisplayItemEntry &entry)
{
    CDisplayItem &di = entry.m_Item;

    const CDisplayInfo &info = command_at(s_ItemTypeIndex);
    for (size_t i = 0; i < commands().size(); i++)
    {
        if (commands()[i].Name == info.Name)
        {
            di.SetType(commands()[i].Type);
            break;
        }
    }

    di.SetText(s_ItemText);
    di.SetGraphic((ushort)CAssistant::TextToGraphic(s_ItemGraphic));
    di.SetHue((ushort)CAssistant::TextToGraphic(s_ItemHue));
    di.SetMidValue(s_ItemMidValue);
    di.SetMinValue(s_ItemMinValue);

    g_AssistantConfig.SaveDisplayItems();
    s_TitleDirty = true; // RecompilleData + RedrawTitle
}

bool item_name_in_use(const char *name, int skip)
{
    const vector<CDisplayItemEntry> &items = g_AssistantConfig.m_DisplayItems;
    for (int i = 0; i < (int)items.size(); i++)
    {
        if (i != skip && ListNamesEqual(items[i].Name, name))
        {
            return true;
        }
    }
    return false;
}

bool group_name_in_use(const char *name, int skip)
{
    const vector<CDisplayGroupEntry> &groups = g_AssistantConfig.m_DisplayGroups;
    for (int i = 0; i < (int)groups.size(); i++)
    {
        if (i != skip && ListNamesEqual(groups[i].Name, name))
        {
            return true;
        }
    }
    return false;
}

const char *DisplayItemNameOf(const CDisplayItemEntry &entry)
{
    return entry.Name.c_str();
}

void DisplayItemSetNameOf(CDisplayItemEntry &entry, const char *name)
{
    entry.Name = name;
}

const char *DisplayGroupNameOf(const CDisplayGroupEntry &entry)
{
    return entry.Name.c_str();
}

void DisplayGroupSetNameOf(CDisplayGroupEntry &entry, const char *name)
{
    entry.Name = name;
}

// was RedrawTitle: map the config radio to the mode and ask for a redraw
void redraw_title()
{
    s_TitleDirty = true;
}

// was RecompilleData. Tracks s_CompiledFrom here as
// well: the Title-data keystroke path recompiles through this, and the
// TabDisplay_Draw sync pass must not recompile + reseed s_TitleData (the
// in-flight InputText state) again on the next frame.
void recompile_title()
{
    s_CompiledFrom = g_AssistantConfig.DisplayTitleData() + "|" +
        std::to_string(g_AssistantConfig.DisplayTextColor());
    g_DisplayManager.RecompilleData(
        g_AssistantConfig.DisplayTitleData(), g_AssistantConfig.DisplayTextColor());
    redraw_title();
}

// --- Configuration page ------------------------------------------------------

void draw_configuration_page()
{
    // "Insert command" (pb_DisplayInsertCommand + the context menu)
    if (ImGui::Button("Insert command"))
    {
        ImGui::OpenPopup("##insert_command");
    }
    if (ImGui::BeginPopup("##insert_command"))
    {
        // the original menu: "Custom items" submenu + the built-in commands
        if (ImGui::BeginMenu("Custom items"))
        {
            const vector<CDisplayItemEntry> &items = g_AssistantConfig.m_DisplayItems;
            for (int i = 0; i < (int)items.size(); i++)
            {
                if (ImGui::MenuItem(items[i].Name.c_str()))
                {
                    // was InsertDisplayAction: the item name is the tag
                    const string tag = "{" + items[i].Name + "}";
                    const size_t len = strlen(s_TitleData);
                    if (len + tag.length() < sizeof(s_TitleData))
                    {
                        strcat(s_TitleData, tag.c_str());
                        g_AssistantConfig.m_DisplayTitleData = s_TitleData;
                        recompile_title();
                    }
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        for (size_t i = 0; i < commands().size(); i++)
        {
            if (commands()[i].Name.length() != 0 &&
                ImGui::MenuItem(commands()[i].Name.c_str()))
            {
                // was InsertDisplayAction: the COMMAND is the tag
                const string tag = "{" + commands()[i].Command + "}";
                const size_t len = strlen(s_TitleData);
                if (len + tag.length() < sizeof(s_TitleData))
                {
                    strcat(s_TitleData, tag.c_str());
                    g_AssistantConfig.m_DisplayTitleData = s_TitleData;
                    recompile_title();
                }
            }
        }
        ImGui::EndPopup();
    }

    // the radio group "Display data on title" (rb_Display* -> m_DisplayType)
    ImGui::TextUnformatted("Display data on title");
    int type = g_AssistantConfig.DisplayType();
    bool typeChanged = false;
    typeChanged |= ImGui::RadioButton("Disabled##display", &type, 0);
    typeChanged |= ImGui::RadioButton("Textual##display", &type, 1);
    typeChanged |= ImGui::RadioButton("Colored textual##display", &type, 2);
    typeChanged |= ImGui::RadioButton("Colored graphical##display", &type, 3);
    if (typeChanged)
    {
        g_AssistantConfig.m_DisplayType = type;
        g_AssistantConfig.SaveMainOptions(); // the display section of Options.xml
        redraw_title();
    }

    // the flag checkboxes (cb_DisplayDoubleImage 0x01 / ...OnName 0x02 /
    // cb_DisplayHightFont 0x04 / cb_HighlightReagents 0x08)
    uint flags = g_AssistantConfig.DisplayFlags();
    bool flagsChanged = false;
    bool flagValue;
    flagValue = (flags & 0x01) != 0;
    if (ImGui::Checkbox("Double image if item stacked", &flagValue))
    {
        flags = flagValue ? (flags | 0x01) : (flags & ~0x01u);
        flagsChanged = true;
    }
    flagValue = (flags & 0x02) != 0;
    if (ImGui::Checkbox("Notoriety color on player name", &flagValue))
    {
        flags = flagValue ? (flags | 0x02) : (flags & ~0x02u);
        flagsChanged = true;
    }
    flagValue = (flags & 0x04) != 0;
    if (ImGui::Checkbox("Hight font", &flagValue))
    {
        flags = flagValue ? (flags | 0x04) : (flags & ~0x04u);
        flagsChanged = true;
    }
    flagValue = (flags & 0x08) != 0;
    if (ImGui::Checkbox("Highlight reagents", &flagValue))
    {
        flags = flagValue ? (flags | 0x08) : (flags & ~0x08u);
        flagsChanged = true;
    }
    if (flagsChanged)
    {
        g_AssistantConfig.m_DisplayFlags = flags;
        g_AssistantConfig.SaveMainOptions();
        redraw_title();
    }

    // the color swatches (pb_DisplayBackgroundColor / pb_DisplayTextColor)
    uint bgcolor = g_AssistantConfig.DisplayBackgroundColor();
    ImGui::TextUnformatted("Background color:");
    ImGui::SameLine();
    if (color_field("##display_bg", &bgcolor))
    {
        g_AssistantConfig.m_DisplayBackgroundColor = bgcolor;
        g_AssistantConfig.SaveMainOptions();
        redraw_title();
    }
    uint textcolor = g_AssistantConfig.DisplayTextColor();
    ImGui::TextUnformatted("Text color:");
    ImGui::SameLine();
    if (color_field("##display_text", &textcolor))
    {
        g_AssistantConfig.m_DisplayTextColor = textcolor;
        g_AssistantConfig.SaveMainOptions();
        recompile_title(); // the text color rides on the compiled commands
    }

    // te_DisplayTitleData ("Title data:")
    ImGui::TextUnformatted("Title data:");
    if (ImGui::InputTextMultiline(
            "##title_data", s_TitleData, sizeof(s_TitleData), ImVec2(-1.0f, 120.0f)))
    {
        // was on_te_DisplayTitleData_textChanged: recompile + redraw; the
        // text persists with the profile (the original saved Options.xml on close)
        g_AssistantConfig.m_DisplayTitleData = s_TitleData;
        AssistantUI_MarkOptionsDirty();
        recompile_title();
    }

    // "Set title from group" (pb_SetTitleGroup + its context menu)
    if (ImGui::Button("Set title from group"))
    {
        ImGui::OpenPopup("##set_title_group");
    }
    if (ImGui::BeginPopup("##set_title_group"))
    {
        const vector<CDisplayGroupEntry> &groups = g_AssistantConfig.m_DisplayGroups;
        for (int i = 0; i < (int)groups.size(); i++)
        {
            if (ImGui::MenuItem(groups[i].Name.c_str()))
            {
                // was InsertDisplayGroupAction -> SetTitleData(text)
                snprintf(s_TitleData, sizeof(s_TitleData), "%s", groups[i].TitleData.c_str());
                g_AssistantConfig.m_DisplayTitleData = s_TitleData;
                AssistantUI_MarkOptionsDirty();
                recompile_title();
            }
        }
        ImGui::EndPopup();
    }
}

// --- Items editor page --------------------------------------------------------

void draw_items_editor_page()
{
    vector<CDisplayItemEntry> &items = g_AssistantConfig.m_DisplayItems;

    const int buttons = ListButtons("display_items");
    if ((buttons & XALB_TARGET) != 0)
    {
        // was on_pb_DisplayItemFromTarget_clicked
        s_FtPending = true;
        g_TargetHandler = &CAssistant::HandleTargetDisplayItem;
        g_Assistant.RequestTarget("Select a object for obtain graphic and color");
    }
    XA_FROM_TARGET_RESULT ftResult;
    if (UI_PollFromTarget(s_FtPending, s_FtGeneration, XA_FROM_TARGET_DISPLAY_ITEM, ftResult))
    {
        // was SetDisplayItemFromTarget: fill the graphic + hue fields
        if (ftResult.Graphic != 0u)
        {
            snprintf(s_ItemGraphic, sizeof(s_ItemGraphic), "%s", CAssistant::GraphicToText(ftResult.Graphic).c_str());
            snprintf(s_ItemHue, sizeof(s_ItemHue), "%s", CAssistant::GraphicToText(ftResult.Color).c_str());
        }
    }

    NameList("##display_items", items, &s_SelectedItem, &DisplayItemNameOf,
             &DisplayItemSetNameOf, false, 180.0f);
    if (s_SelectedItem >= (int)items.size())
    {
        s_SelectedItem = -1;
    }

    // selection -> fields (on_lw_DisplayItems_clicked)
    if (s_SelectedItem != s_LastSelectedItem)
    {
        s_LastSelectedItem = s_SelectedItem;
        s_Hint[0] = '\0';
        if (s_SelectedItem >= 0 && s_SelectedItem < (int)items.size())
        {
            snprintf(s_ItemName, sizeof(s_ItemName), "%s", items[s_SelectedItem].Name.c_str());
            item_to_fields(items[s_SelectedItem].m_Item, true);
        }
    }

    ImGui::TextUnformatted("Properties");
    ImGui::Separator();

    // the static-art preview of the selected item (the original list rows carried
    // the decoded icon; the ImGui list core is text-only, so the preview
    // sits with the properties of the selected entry)
    if (s_SelectedItem >= 0 && s_SelectedItem < (int)items.size())
    {
        const CDisplayItem &selItem = items[s_SelectedItem].m_Item;
        const uint key = (uint)((selItem.GetGraphic() << 16) | selItem.GetHue());
        ImTextureData *tex = IconTextureFor(g_DisplayManager.GetIcon(selItem), key);
        if (tex != nullptr)
        {
            const float box = 44.0f;
            const float scale =
                ImMin(box / tex->Width, box / tex->Height);
            const float w = tex->Width * scale;
            const float h = tex->Height * scale;
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddImage(
                tex->GetTexRef(), pos, ImVec2(pos.x + w, pos.y + h));
            ImGui::Dummy(ImVec2(box, box));
            ImGui::SameLine();
        }
    }

    if (ImGui::BeginCombo("Type##display_item", command_at(s_ItemTypeIndex).Name.c_str()))
    {
        const int count = command_count();
        for (int i = 0; i < count; i++)
        {
            if (ImGui::Selectable(command_at(i).Name.c_str(), i == s_ItemTypeIndex))
            {
                s_ItemTypeIndex = i;
                // was on_cb_DisplayItemType_currentIndexChanged: load the
                // DEFAULT item of that type into the fields
                item_to_fields(g_DisplayManager.GetDisplayItemByName(command_at(i).Command), false);
            }
        }
        ImGui::EndCombo();
    }

    bool changed = false;
    ImGui::TextUnformatted("ID:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    changed |= HexEditField("##item_graphic", s_ItemGraphic, sizeof(s_ItemGraphic));
    ImGui::TextUnformatted("Hue:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    changed |= HexEditField("##item_hue", s_ItemHue, sizeof(s_ItemHue));
    ImGui::TextUnformatted("Text:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    changed |= ImGui::InputText("##item_text", s_ItemText, sizeof(s_ItemText));

    // "Value:" (sb_DisplayItemMin/MidValue, max 99999 in the .ui)
    ImGui::TextUnformatted("Value:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::InputInt("Minimum##item", &s_ItemMinValue))
    {
        s_ItemMinValue = s_ItemMinValue < 0 ? 0 : (s_ItemMinValue > 99999 ? 99999 : s_ItemMinValue);
        changed = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::InputInt("Middle##item", &s_ItemMidValue))
    {
        s_ItemMidValue = s_ItemMidValue < 0 ? 0 : (s_ItemMidValue > 99999 ? 99999 : s_ItemMidValue);
        changed = true;
    }

    if (s_SelectedItem >= 0 && s_SelectedItem < (int)items.size())
    {
        // the "With background" / "Calculate colors in percents" checks and
        // the two 3-swatch groups read/write the entry directly (the original page
        // saved only through the Save icon; the checks were bound to the item)
        CDisplayItem &di = items[s_SelectedItem].m_Item;
        bool background = di.GetBackground();
        if (ImGui::Checkbox("With background", &background))
        {
            di.SetBackground(background);
            changed = true;
        }
        ImGui::SameLine();
        bool percents = di.GetPercents();
        if (ImGui::Checkbox("Calculate colors in percents", &percents))
        {
            di.SetPercents(percents);
            changed = true;
        }

        // Colors: "Text:" normal/mid/min + "Bg:" normal/mid/min (the label
        // grid of 6 color swatch buttons)
        ImGui::TextUnformatted("Colors - Text:");
        ImGui::SameLine();
        uint c = di.GetColor();
        changed |= color_field("##item_text_c", &c);
        di.SetColor(c);
        ImGui::SameLine();
        c = di.GetMidColor();
        changed |= color_field("##item_text_mc", &c);
        di.SetMidColor(c);
        ImGui::SameLine();
        c = di.GetMinColor();
        changed |= color_field("##item_text_min", &c);
        di.SetMinColor(c);
        ImGui::TextUnformatted("Colors - Bg:");
        ImGui::SameLine();
        c = di.GetBackgroundColor();
        changed |= color_field("##item_bg_c", &c);
        di.SetBackgroundColor(c);
        ImGui::SameLine();
        c = di.GetBackgroundMidColor();
        changed |= color_field("##item_bg_mc", &c);
        di.SetBackgroundMidColor(c);
        ImGui::SameLine();
        c = di.GetBackgroundMinColor();
        changed |= color_field("##item_bg_min", &c);
        di.SetBackgroundMinColor(c);
    }

    // Name: field (le_DisplayItemName)
    ImGui::TextUnformatted("Name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##item_name", s_ItemName, sizeof(s_ItemName));

    // Create / Save / Remove (the pb_DisplayItem* icon buttons)
    if ((buttons & XALB_ADD) != 0)
    {
        if (s_ItemName[0] == '\0')
        {
            snprintf(s_Hint, sizeof(s_Hint), "Enter the display item name!");
        }
        else if (item_name_in_use(s_ItemName, -1))
        {
            snprintf(s_Hint, sizeof(s_Hint), "Display item name is already exists!");
        }
        else
        {
            CDisplayItemEntry entry;
            entry.Name = s_ItemName;
            // push BEFORE the save: the new entry must already sit in the
            // vector when SaveDisplayItems writes the file (fields_to_item's
            // own save is not the only one - kept explicit here)
            items.push_back(entry);
            s_SelectedItem = s_LastSelectedItem = (int)items.size() - 1;
            fields_to_item(items[s_SelectedItem]); // fields -> the entry + marks the title dirty
            g_AssistantConfig.SaveDisplayItems();
            s_Hint[0] = '\0';
        }
    }
    if ((buttons & XALB_SAVE) != 0)
    {
        if (s_ItemName[0] == '\0')
        {
            snprintf(s_Hint, sizeof(s_Hint), "Enter the display item name!");
        }
        else if (s_SelectedItem < 0 || s_SelectedItem >= (int)items.size())
        {
            snprintf(s_Hint, sizeof(s_Hint), "Item is not selected!");
        }
        else if (item_name_in_use(s_ItemName, s_SelectedItem))
        {
            snprintf(s_Hint, sizeof(s_Hint), "Display item name is already exists (not this item)!");
        }
        else
        {
            items[s_SelectedItem].Name = s_ItemName;
            fields_to_item(items[s_SelectedItem]);
            s_Hint[0] = '\0';
        }
    }
    if ((buttons & XALB_REMOVE) != 0 && s_SelectedItem >= 0 && s_SelectedItem < (int)items.size())
    {
        // was on_pb_DisplayItemRemove_clicked: save + recompile
        items.erase(items.begin() + s_SelectedItem);
        s_SelectedItem = s_LastSelectedItem = -1;
        g_AssistantConfig.SaveDisplayItems();
        recompile_title();
    }
    if (changed && s_SelectedItem >= 0 && s_SelectedItem < (int)items.size())
    {
        // the checkbox edits write through immediately; the color swatches
        // stay live-preview during a drag and land here only on the release
        // frame (color_field commits on IsItemDeactivatedAfterEdit), so a
        // swatch drag writes DisplayItems.xml once instead of every frame
        g_AssistantConfig.SaveDisplayItems();
        s_TitleDirty = true;
    }
    if (s_Hint[0] != '\0')
    {
        ImGui::TextDisabled("%s", s_Hint);
    }
}

// --- Group editor page ---------------------------------------------------------

void draw_group_editor_page()
{
    vector<CDisplayGroupEntry> &groups = g_AssistantConfig.m_DisplayGroups;

    const int buttons = ListButtons("display_groups");
    NameList("##display_groups", groups, &s_SelectedGroup, &DisplayGroupNameOf,
             &DisplayGroupSetNameOf, false, 180.0f);
    if (s_SelectedGroup >= (int)groups.size())
    {
        s_SelectedGroup = -1;
    }

    // selection -> fields (on_lw_DisplayGroups_clicked)
    static int lastSelected = -2;
    if (s_SelectedGroup != lastSelected)
    {
        lastSelected = s_SelectedGroup;
        if (s_SelectedGroup >= 0 && s_SelectedGroup < (int)groups.size())
        {
            snprintf(s_GroupName, sizeof(s_GroupName), "%s", groups[s_SelectedGroup].Name.c_str());
            snprintf(s_GroupData, sizeof(s_GroupData), "%s", groups[s_SelectedGroup].TitleData.c_str());
        }
    }

    ImGui::TextUnformatted("Name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##group_name", s_GroupName, sizeof(s_GroupName));
    ImGui::TextUnformatted("Title data:");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextMultiline("##group_data", s_GroupData, sizeof(s_GroupData), ImVec2(-1.0f, 90.0f));

    if ((buttons & XALB_ADD) != 0)
    {
        // was on_pb_DisplayGroupCreate_clicked
        if (s_GroupName[0] == '\0')
        {
            snprintf(s_Hint, sizeof(s_Hint), "Enter the title group name!");
        }
        else if (group_name_in_use(s_GroupName, -1))
        {
            snprintf(s_Hint, sizeof(s_Hint), "Display group name is already exists!");
        }
        else
        {
            CDisplayGroupEntry entry;
            entry.Name = s_GroupName;
            entry.TitleData = s_GroupData;
            groups.push_back(entry);
            s_SelectedGroup = (int)groups.size() - 1;
            g_AssistantConfig.SaveDisplayGroups();
            recompile_title();
            s_Hint[0] = '\0';
        }
    }
    if ((buttons & XALB_SAVE) != 0)
    {
        // was on_pb_DisplayGroupSave_clicked
        if (s_GroupName[0] == '\0')
        {
            snprintf(s_Hint, sizeof(s_Hint), "Enter the title group name!");
        }
        else if (s_SelectedGroup < 0 || s_SelectedGroup >= (int)groups.size())
        {
            snprintf(s_Hint, sizeof(s_Hint), "Group is not selected!");
        }
        else if (group_name_in_use(s_GroupName, s_SelectedGroup))
        {
            snprintf(s_Hint, sizeof(s_Hint), "Display group name is already exists (not this item)!");
        }
        else
        {
            groups[s_SelectedGroup].Name = s_GroupName;
            groups[s_SelectedGroup].TitleData = s_GroupData;
            g_AssistantConfig.SaveDisplayGroups();
            recompile_title();
            s_Hint[0] = '\0';
        }
    }
    if ((buttons & XALB_REMOVE) != 0 && s_SelectedGroup >= 0 && s_SelectedGroup < (int)groups.size())
    {
        // was on_pb_DisplayGroupRemove_clicked
        groups.erase(groups.begin() + s_SelectedGroup);
        s_SelectedGroup = -1;
        g_AssistantConfig.SaveDisplayGroups();
        recompile_title();
    }
    if (s_Hint[0] != '\0')
    {
        ImGui::TextDisabled("%s", s_Hint);
    }
}

} // namespace

void TabDisplay_Draw()
{
    // keep the compile state in sync with the config (profile load/switch)
    const string key = g_AssistantConfig.DisplayTitleData() + "|" +
        std::to_string(g_AssistantConfig.DisplayTextColor());
    if (key != s_CompiledFrom)
    {
        s_CompiledFrom = key;
        snprintf(s_TitleData, sizeof(s_TitleData), "%s", g_AssistantConfig.DisplayTitleData().c_str());
        recompile_title();
    }
    if (ImGui::BeginTabBar("##tw_display", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Configuration"))
        {
            draw_configuration_page();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Items editor"))
        {
            draw_items_editor_page();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Group editor"))
        {
            draw_group_editor_page();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

bool TabDisplay_ConsumeTitleDirty()
{
    if (!s_TitleDirty)
    {
        return false;
    }
    s_TitleDirty = false;
    return true;
}

// Shutdown: frees every cached item-preview icon texture (CPU delete AFTER the
// ImGui context is gone - the same Phase 2 ordering as the atlas / AssMap /
// logo: the backend reclaimed the GL names inside ui_shutdown by walking the
// registered textures with RefCount == 1). Entries evicted at runtime but not
// yet reaped are plain CPU memory by now as well.
void TabDisplay_AfterImGuiShutdown()
{
    for (auto &kv : s_IconTextures)
    {
        delete kv.second.tex;
    }
    s_IconTextures.clear();
    for (ImTextureData *tex : s_IconRetired)
    {
        delete tex;
    }
    s_IconRetired.clear();
}

void TabDisplay_RequestTitleRedraw()
{
    s_TitleDirty = true;
}

void TabDisplay_CancelPendingFromTargets()
{
    s_FtPending = false;
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
