// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See scripteditor.h. File dialogs: the plan §8.2 rule (no native dialog
// dependency) - a small in-ImGui file picker over std::filesystem rooted at
// the current directory replaces the native open/save file dialogs.

#include "scripteditor.h"
#include "editor.h"

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include <external/gfx/imgui/imgui.h>

#include "../config/assistantconfig.h"
#include "../core/assist.h"
#include "scriptmenu.h"
#include "../script/scripthandle.h"

#include <sys/stat.h>

#include <algorithm>
#include <cstring>
#include <dirent.h>

namespace xuoassist {

namespace {

char s_FindText[128] = { 0 };
char s_ReplaceText[128] = { 0 };
int s_GotoLine = 1;
bool s_GotoPending = false;
bool s_ShowFind = false;
bool s_ShowReplace = false;
bool s_ShowGoto = false;
bool s_ShowFilePicker = false;
bool s_FilePickerSave = false;
char s_FilePickerName[256] = { 0 };
astr_t s_FilePickerDir = ".";
astr_t s_Status = "Ready";
astr_t s_PendingOpenPath;

// tiny in-ImGui file picker over std::filesystem-like readdir (plan §8.2):
// lists the directory, `..` to go up, double-click a .oajs/.js file to pick.
bool draw_file_picker()
{
    bool picked = false;
    if (!s_ShowFilePicker)
    {
        return false;
    }
    ImGui::SetNextWindowSize(ImVec2(420, 320), ImGuiCond_Appearing);
    if (!ImGui::Begin("Select script file", &s_ShowFilePicker))
    {
        ImGui::End();
        return false;
    }

    if (ImGui::Button("Up"))
    {
        size_t slash = s_FilePickerDir.find_last_of('/');
        s_FilePickerDir = slash == astr_t::npos ? "." : s_FilePickerDir.substr(0, slash);
        if (s_FilePickerDir.empty())
        {
            s_FilePickerDir = "/";
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(s_FilePickerDir.c_str());
    ImGui::Separator();

    DIR *dir = opendir(s_FilePickerDir.c_str());
    if (dir != nullptr)
    {
        struct dirent *entry = nullptr;
        vector<astr_t> directories;
        vector<astr_t> files;
        while ((entry = readdir(dir)) != nullptr)
        {
            const astr_t name = entry->d_name;
            if (name == "." || name.empty())
            {
                continue;
            }
            const astr_t full = s_FilePickerDir + "/" + name;
            struct stat st = {};
            if (stat(full.c_str(), &st) != 0)
            {
                continue;
            }
            if (S_ISDIR(st.st_mode))
            {
                directories.push_back(name);
            }
            // size guards BEFORE the compare: size()-5 underflows for 4-char
            // names (".oajs" check used to throw std::out_of_range)
            else if ((name.size() >= 5 && name.compare(name.size() - 5, 5, ".oajs") == 0) ||
                (name.size() >= 3 && name.compare(name.size() - 3, 3, ".js") == 0))
            {
                files.push_back(name);
            }
        }
        closedir(dir);

        std::sort(directories.begin(), directories.end());
        std::sort(files.begin(), files.end());
        for (const astr_t &name : directories)
        {
            if (ImGui::Selectable((name + "/").c_str()))
            {
                s_FilePickerDir = s_FilePickerDir == "/" ? "/" + name : s_FilePickerDir + "/" + name;
            }
        }
        for (const astr_t &name : files)
        {
            if (ImGui::Selectable(name.c_str()))
            {
                snprintf(s_FilePickerName, sizeof(s_FilePickerName), "%s", name.c_str());
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                picked = true;
            }
        }
    }

    ImGui::Separator();
    ImGui::SetNextItemWidth(-70);
    ImGui::InputText("##filename", s_FilePickerName, sizeof(s_FilePickerName));
    ImGui::SameLine();
    if (ImGui::Button(s_FilePickerSave ? "Save" : "Open") &&
        s_FilePickerName[0] != 0)
    {
        picked = true;
    }
    ImGui::End();
    return picked;
}

bool read_file(const astr_t &path, astr_t &out)
{
    FILE *file = fopen(path.c_str(), "rb");
    if (file == nullptr)
    {
        return false;
    }
    char buffer[4096];
    size_t read = 0;
    while ((read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        out.append(buffer, read);
    }
    fclose(file);
    return true;
}

bool write_file(const astr_t &path, const astr_t &text)
{
    // the script editor's Save button: the crash-safe temp+rename path so a
    // crash mid-save cannot truncate a user script (plan §8.1)
    return AtomicFileWrite(fs_path_from(path), text.data(), text.size());
}

void open_path(const astr_t &path)
{
    astr_t text;
    if (read_file(path, text))
    {
        ScriptEditor_SetText(text); // bumps the buffer generation (re-split)
        ScriptEditor_SetPath(path);
        s_Status = path;
    }
    else
    {
        s_Status = "Cannot open: " + path;
    }
}

void save_path(const astr_t &path)
{
    if (write_file(path, ScriptEditor_Text()))
    {
        ScriptEditor_SetPath(path);
        s_Status = "Saved: " + path;
    }
    else
    {
        s_Status = "Cannot save: " + path;
    }
}

} // namespace

void ScriptEditorWindow_Draw(bool *open)
{
    // the X button only FLIPS the flag (ImGui p_open reports the click, it
    // does not hide anything): the window stays closed until the Scripts tab
    // "Open editor" button sets the flag again. The buffer is
    // shared with the script runtime, so content survives close/reopen
    // (the original dialog had no closeEvent override either: plain hide).
    if (open == nullptr || !*open)
    {
        return;
    }

    if (!s_PendingOpenPath.empty())
    {
        open_path(s_PendingOpenPath);
        s_PendingOpenPath.clear();
    }

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Script editor", open))
    {
        ImGui::End();
        return;
    }

    // --- toolbar (scripteditordialog.ui) -----------------------------------
    if (ImGui::Button("New"))
    {
        ScriptEditor_SetText(""); // bumps the buffer generation (re-split)
        ScriptEditor_SetPath("");
        s_Status = "New script";
    }
    if (ImGui::Button("Open"))
    {
        s_ShowFilePicker = true;
        s_FilePickerSave = false;
        s_FilePickerDir = ScriptEditor_Path().empty() ? "."
            : (s_FilePickerDir = ScriptEditor_Path().substr(0, ScriptEditor_Path().find_last_of('/')));
    }
    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        // TabScripts::SaveScript fallback: an empty path saves Autoload.oajs
        const astr_t path = ScriptEditor_Path().empty() ? "./Autoload.oajs" : ScriptEditor_Path();
        save_path(path);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As"))
    {
        s_ShowFilePicker = true;
        s_FilePickerSave = true;
        snprintf(s_FilePickerName, sizeof(s_FilePickerName), "script.oajs");
    }
    ImGui::SameLine();
    if (ImGui::Button("Find"))
    {
        s_ShowFind = true;
        ImGui::OpenPopup("Find text");
    }
    ImGui::SameLine();
    if (ImGui::Button("GoTo"))
    {
        s_ShowGoto = true;
        ImGui::OpenPopup("Go to line");
    }
    ImGui::SameLine();
    if (ImGui::Button("Replace"))
    {
        s_ShowReplace = true;
        ImGui::OpenPopup("Replace text");
    }
    ImGui::SameLine();
    if (ImGui::Button("Comment"))
    {
        TextEditor_CommentSelection();
    }
    ImGui::SameLine();
    // the command insert menu (the original giant context menu, now data-driven)
    if (ImGui::Button("Commands"))
    {
        ImGui::OpenPopup("ScriptCommandMenu");
    }
    astr_t inserted;
    if (ScriptMenu_Draw("ScriptCommandMenu", inserted))
    {
        TextEditor_InsertAtCursor(inserted);
    }

    // --- the Find popup (assist/textoperationdialog find, 393x113) ---------
    if (ImGui::BeginPopup("Find text"))
    {
        static bool down = true;
        static bool caseSensitive = true;
        ImGui::SetNextItemWidth(220);
        ImGui::InputText("##find", s_FindText, sizeof(s_FindText));
        ImGui::Checkbox("Case sensitive", &caseSensitive);
        ImGui::Checkbox("Down (search direction)", &down);
        if (ImGui::Button("Search next") || ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            s_Status = TextEditor_Find(s_FindText, down, caseSensitive) ?
                "Found" :
                "Not found";
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // --- the Replace popup (textoperationdialog replace, 397x134) ----------
    if (ImGui::BeginPopup("Replace text"))
    {
        static bool caseSensitive = true;
        ImGui::SetNextItemWidth(220);
        ImGui::InputText("Text", s_FindText, sizeof(s_FindText));
        ImGui::SetNextItemWidth(220);
        ImGui::InputText("Replace with", s_ReplaceText, sizeof(s_ReplaceText));
        ImGui::Checkbox("Case sensitive", &caseSensitive);
        if (ImGui::Button("Search next"))
        {
            s_Status = TextEditor_Find(s_FindText, true, caseSensitive) ? "Found" : "Not found";
        }
        ImGui::SameLine();
        if (ImGui::Button("Replace"))
        {
            TextEditor_Replace(s_FindText, s_ReplaceText, caseSensitive);
        }
        ImGui::SameLine();
        if (ImGui::Button("Replace all"))
        {
            s_Status = TextEditor_ReplaceAll(s_FindText, s_ReplaceText, caseSensitive) ?
                "Replaced" :
                "Not found";
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // --- the Go to line popup (textoperationdialog gotoline, 206x73) -------
    if (ImGui::BeginPopup("Go to line"))
    {
        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Line index", &s_GotoLine);
        if (ImGui::Button("Go") || ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            s_GotoPending = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // --- the editor ---------------------------------------------------------
    TextEditor_Draw("ScriptEditorText", &ScriptEditor_Text(), 0.0f, s_GotoLine, &s_GotoPending);

    // --- the statusbar -------------------------------------------------------
    ImGui::Separator();
    const vector<astr_t> functions = ScriptEditor_FunctionList();
    ImGui::Text("%s | %d lines | %d functions", s_Status.c_str(),
        (int)(std::count(ScriptEditor_Text().begin(), ScriptEditor_Text().end(), '\n') + 1),
        (int)functions.size());

    if (draw_file_picker())
    {
        const astr_t path = (s_FilePickerDir == "/" ? "/" : s_FilePickerDir + "/") + s_FilePickerName;
        if (s_FilePickerSave)
        {
            save_path(path);
        }
        else
        {
            open_path(path);
        }
        s_ShowFilePicker = false;
    }

    ImGui::End();
}

} // namespace xuoassist
