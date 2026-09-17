// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The three Filters sub-tabs: Speech, Replaces (with the Mode/Type combos),
// Sound. Entries bind to the g_AssistantConfig filter containers; the packet
// handlers read exactly these through CheckSpeechInFilter / CheckReplaceText /
// CheckSoundInFilter.

#include "assistantui.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include "widgets.h"
#include "../config/assistantconfig.h"
#include "../core/assist.h"

namespace xuoassist {

namespace {


static const char *SpeechTextOf(const CFilterSpeechEntry &e) { return e.Text.c_str(); }
static void SpeechSetTextOf(CFilterSpeechEntry &e, const char *n) { e.Text = n; }
static const char *ReplaceTextOf(const CFilterReplaceEntry &e) { return e.OriginalText.c_str(); }
static void ReplaceSetTextOf(CFilterReplaceEntry &e, const char *n) { e.OriginalText = n; }
static const char *SoundNameOf(const CFilterSoundEntry &e) { return e.Name.c_str(); }
static void SoundSetNameOf(CFilterSoundEntry &e, const char *n) { e.Name = n; }

const ImVec4 HINT_COLOR = ImVec4(0.9f, 0.3f, 0.3f, 1.0f);

// "Mode:" combo (tabfiltersreplaces.ui cb_FilterReplaceMode / RTM_*)
const char *const s_ReplaceModes[] = { "All", "Friends&You", "Enemies", "Others" };
// "Type:" combo (cb_FilterReplaceType / RTT_*)
const char *const s_ReplaceTypes[] = { "All", "ASCII", "Unicode" };

// --- Speech (tabfiltersspeech.cpp) ------------------------------------------------

void draw_speech_tab()
{
    const char *const PANE_SEED = "speech";
    static int selected = -1;
    static int lastSelected = -1;
    static char text[128] = { 0 };
    static char color[16] = { 0 };
    static bool fullMatch = false;
    static char hint[128] = { 0 };

    vector<CFilterSpeechEntry> &items = g_AssistantConfig.m_SpeechFilters;
    const int buttons = ListButtons(PANE_SEED);
    bool changed = NameList("##speech_filters", items, &selected, &SpeechTextOf, &SpeechSetTextOf);
    if (selected != lastSelected)
    {
        lastSelected = selected;
        hint[0] = '\0';
        if (selected >= 0 && selected < (int)items.size())
        {
            snprintf(text, sizeof(text), "%s", items[selected].Text.c_str());
            snprintf(color, sizeof(color), "%s", items[selected].Color.c_str());
            fullMatch = items[selected].FullMatch;
        }
    }

    // the static edit buffer is read directly (draw path: no scratch copy)
    if ((buttons & XALB_ADD) != 0)
    {
        if (text[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the text!");
        }
        else
        {
            CFilterSpeechEntry entry;
            entry.Text = text;
            entry.Color = color;
            entry.FullMatch = fullMatch;
            items.push_back(entry);
            selected = lastSelected = (int)items.size() - 1;
            changed = true;
            hint[0] = '\0';
        }
    }
    if ((buttons & XALB_SAVE) != 0)
    {
        if (text[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the text!");
        }
        else if (selected < 0 || selected >= (int)items.size())
        {
            snprintf(hint, sizeof(hint), "Entry is not selected!");
        }
        else
        {
            items[selected].Text = text;
            items[selected].Color = color;
            items[selected].FullMatch = fullMatch;
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
        g_AssistantConfig.SaveSpeechFilter();
    }

    ImGui::TextUnformatted("Text:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##speech_text", text, sizeof(text));
    ImGui::TextUnformatted("Color:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    HexEditField("##speech_color", color, sizeof(color));
    ImGui::SameLine();
    if (ImGui::Checkbox("Full match", &fullMatch))
    {
        if (selected >= 0 && selected < (int)items.size())
        {
            items[selected].FullMatch = fullMatch;
            g_AssistantConfig.SaveSpeechFilter();
        }
    }
    if (hint[0] != '\0')
    {
        ImGui::TextColored(HINT_COLOR, "%s", hint);
    }
}

// --- Replaces (tabfiltersreplaces.cpp) ---------------------------------------------

void draw_replaces_tab()
{
    const char *const PANE_SEED = "replaces";
    static int selected = -1;
    static int lastSelected = -1;
    static char originalText[128] = { 0 };
    static char originalColor[16] = { 0 };
    static char replacedText[128] = { 0 };
    static char replacedColor[16] = { 0 };
    static int type = RTT_ALL;
    static int mode = RTM_ALL;
    static bool fullMatch = false;
    static char hint[128] = { 0 };

    vector<CFilterReplaceEntry> &items = g_AssistantConfig.m_Replaces;
    const int buttons = ListButtons(PANE_SEED);
    bool changed = NameList("##replace_filters", items, &selected, &ReplaceTextOf, &ReplaceSetTextOf);
    if (selected != lastSelected)
    {
        lastSelected = selected;
        hint[0] = '\0';
        if (selected >= 0 && selected < (int)items.size())
        {
            snprintf(originalText, sizeof(originalText), "%s", items[selected].OriginalText.c_str());
            snprintf(originalColor, sizeof(originalColor), "%s", items[selected].OriginalColor.c_str());
            snprintf(replacedText, sizeof(replacedText), "%s", items[selected].ReplacedText.c_str());
            snprintf(replacedColor, sizeof(replacedColor), "%s", items[selected].ReplacedColor.c_str());
            type = items[selected].Type;
            mode = items[selected].Mode;
            fullMatch = items[selected].FullMatch;
        }
    }

    // the static edit buffers are read directly (draw path: no scratch copies)
    if ((buttons & XALB_ADD) != 0)
    {
        if (originalText[0] == '\0' || replacedText[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the original and replace texts!");
        }
        else
        {
            CFilterReplaceEntry entry;
            entry.OriginalText = originalText;
            entry.OriginalColor = originalColor;
            entry.ReplacedText = replacedText;
            entry.ReplacedColor = replacedColor;
            entry.Type = type;
            entry.Mode = mode;
            entry.FullMatch = fullMatch;
            items.push_back(entry);
            selected = lastSelected = (int)items.size() - 1;
            changed = true;
            hint[0] = '\0';
        }
    }
    if ((buttons & XALB_SAVE) != 0)
    {
        if (originalText[0] == '\0' || replacedText[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the original and replace texts!");
        }
        else if (selected < 0 || selected >= (int)items.size())
        {
            snprintf(hint, sizeof(hint), "Entry is not selected!");
        }
        else
        {
            items[selected].OriginalText = originalText;
            items[selected].OriginalColor = originalColor;
            items[selected].ReplacedText = replacedText;
            items[selected].ReplacedColor = replacedColor;
            items[selected].Type = type;
            items[selected].Mode = mode;
            items[selected].FullMatch = fullMatch;
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
        g_AssistantConfig.SaveTextReplaces();
    }

    ImGui::TextUnformatted("Original text:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("##replace_original", originalText, sizeof(originalText));
    ImGui::TextUnformatted("Color:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    HexEditField("##replace_original_color", originalColor, sizeof(originalColor));
    ImGui::TextUnformatted("Replace text:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("##replace_replaced", replacedText, sizeof(replacedText));
    ImGui::TextUnformatted("Color:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    HexEditField("##replace_replaced_color", replacedColor, sizeof(replacedColor));
    if (ImGui::Checkbox("Full match", &fullMatch))
    {
        if (selected >= 0 && selected < (int)items.size())
        {
            items[selected].FullMatch = fullMatch;
            g_AssistantConfig.SaveTextReplaces();
        }
    }
    ImGui::TextUnformatted("Mode:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ComboGroup("##replace_mode", &mode, s_ReplaceModes, 4))
    {
        if (selected >= 0 && selected < (int)items.size())
        {
            items[selected].Mode = mode;
            g_AssistantConfig.SaveTextReplaces();
        }
    }
    ImGui::TextUnformatted("Type:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ComboGroup("##replace_type", &type, s_ReplaceTypes, 3))
    {
        if (selected >= 0 && selected < (int)items.size())
        {
            items[selected].Type = type;
            g_AssistantConfig.SaveTextReplaces();
        }
    }
    if (hint[0] != '\0')
    {
        ImGui::TextColored(HINT_COLOR, "%s", hint);
    }
}

// --- Sound (tabfilterssound.cpp) -----------------------------------------------------

void draw_sound_tab()
{
    const char *const PANE_SEED = "sound";
    static int selected = -1;
    static int lastSelected = -1;
    static char index[16] = { 0 };
    static char name[64] = { 0 };
    static char hint[128] = { 0 };

    vector<CFilterSoundEntry> &items = g_AssistantConfig.m_SoundFilters;
    const int buttons = ListButtons(PANE_SEED);
    bool changed = NameList("##sound_filters", items, &selected, &SoundNameOf, &SoundSetNameOf);
    if (selected != lastSelected)
    {
        lastSelected = selected;
        hint[0] = '\0';
        if (selected >= 0 && selected < (int)items.size())
        {
            snprintf(index, sizeof(index), "%s", items[selected].Index.c_str());
            snprintf(name, sizeof(name), "%s", items[selected].Name.c_str());
        }
    }

    // the static edit buffer is read directly (draw path: no scratch copy)
    if ((buttons & XALB_ADD) != 0)
    {
        if (index[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the sound index!");
        }
        else
        {
            CFilterSoundEntry entry;
            entry.Index = index;
            entry.Name = name;
            items.push_back(entry);
            selected = lastSelected = (int)items.size() - 1;
            changed = true;
            hint[0] = '\0';
        }
    }
    if ((buttons & XALB_SAVE) != 0)
    {
        if (index[0] == '\0')
        {
            snprintf(hint, sizeof(hint), "Enter the sound index!");
        }
        else if (selected < 0 || selected >= (int)items.size())
        {
            snprintf(hint, sizeof(hint), "Entry is not selected!");
        }
        else
        {
            items[selected].Index = index;
            items[selected].Name = name;
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
        g_AssistantConfig.SaveSoundFilter();
    }

    ImGui::TextUnformatted("Index:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    HexEditField("##sound_index", index, sizeof(index));
    ImGui::TextUnformatted("Name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputText("##sound_name", name, sizeof(name));
    if (hint[0] != '\0')
    {
        ImGui::TextColored(HINT_COLOR, "%s", hint);
    }
}

} // namespace

void TabFilters_Draw(int subTab)
{
    switch (subTab)
    {
        case TAB_FILTERS_SPEECH:
            draw_speech_tab();
            break;
        case TAB_FILTERS_REPLACES:
            draw_replaces_tab();
            break;
        case TAB_FILTERS_SOUND:
            draw_sound_tab();
            break;
        default:
            break;
    }
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
