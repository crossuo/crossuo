// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See scripteditor.h. One window per open document (ui/editors.cpp owns the
// list and the save persistence). No Open here: a file is opened by
// double-clicking it in the Scripts tab, and Save As goes through the native
// picker (ui/scriptpicker.cpp).

#include "scripteditor.h"
#include "editor.h"
#include "editors.h"

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include <external/gfx/imgui/imgui.h>

#include "scriptmenu.h"
#include "scriptpicker.h"
#include "../config/assistantconfig.h"
#include "../script/scriptlist.h"
#include "../script/scripthandle.h" // Script_AnalyzeFunctions

#include <algorithm>

namespace xuoassist {

namespace {

void save_path(CEditorDoc *doc, const astr_t &path)
{
    if (ScriptList_WriteFile(path, doc->Text))
    {
        doc->Status = "Saved: " + path;
        Editors_CommitSave(doc);
    }
    else
    {
        doc->Status = "Cannot save: " + path;
    }
}

void save_as(CEditorDoc *doc, const astr_t &suggested)
{
    astr_t picked;
    if (!ScriptPicker_SaveAs(suggested, picked, doc->SaveAsFailedOnce))
    {
        doc->Status = "Save As cancelled";
        return;
    }
    save_path(doc, picked);
    Editors_CommitSaveAs(doc, picked);
}

} // namespace

void ScriptEditor_SyncDocument(const char *id, astr_t *buffer)
{
    TextEditor_SyncBuffer(id, buffer);
}

void ScriptEditorWindow_DrawDoc(CEditorDoc *doc, bool *open)
{
    if (doc == nullptr || open == nullptr || !*open)
    {
        return;
    }

    char title[512];
    snprintf(title, sizeof(title), "Script editor - %s%s###scripteditor_%s",
        doc->Name.c_str(), doc->Dirty ? " *" : "",
        doc->Key.empty() ? doc->Name.c_str() : doc->Key.c_str());

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    // the title carries the dirty marker; the ### ID stays stable so a dirty
    // flip does not reset the window or the widget's line buffer
    if (!ImGui::Begin(title, open))
    {
        ImGui::End();
        return;
    }

    // --- toolbar -----------------------------------------------------------
    if (ImGui::Button("Save"))
    {
        if (doc->Path.empty())
        {
            // untitled: Save is Save As (a fresh document has no target yet)
            save_as(doc, ScriptPicker_JoinPath(ScriptPicker_DefaultDir(), doc->Name));
        }
        else
        {
            save_path(doc, doc->Path);
        }
    }
    ImGui::SetItemTooltip(doc->Path.empty() ? "No file yet: picks one (Save As)"
                                            : "Writes the file on disk");
    ImGui::SameLine();
    if (ImGui::Button("Save As"))
    {
        const astr_t suggested = doc->Path.empty()
            ? ScriptPicker_JoinPath(ScriptPicker_DefaultDir(), doc->Name)
            : doc->Path;
        astr_t picked;
        if (ScriptPicker_SaveAs(suggested, picked, doc->SaveAsFailedOnce))
        {
            save_path(doc, picked);
            Editors_CommitSaveAs(doc, picked);
        }
        else if (doc->Path.empty() && doc->Text.empty())
        {
            // an empty untitled document the user abandoned: close it
            *open = false;
        }
        else
        {
            doc->Status = "Save As cancelled";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Find"))
    {
        ImGui::OpenPopup("Find text");
    }
    ImGui::SameLine();
    if (ImGui::Button("GoTo"))
    {
        ImGui::OpenPopup("Go to line");
    }
    ImGui::SameLine();
    if (ImGui::Button("Replace"))
    {
        ImGui::OpenPopup("Replace text");
    }
    ImGui::SameLine();
    if (ImGui::Button("Comment"))
    {
        TextEditor_CommentSelection();
    }
    ImGui::SameLine();
    if (ImGui::Button("Commands"))
    {
        ImGui::OpenPopup("ScriptCommandMenu");
    }
    astr_t inserted;
    if (ScriptMenu_Draw("ScriptCommandMenu", inserted))
    {
        TextEditor_InsertAtCursor(inserted);
    }

    // --- the Find popup ----------------------------------------------------
    if (ImGui::BeginPopup("Find text"))
    {
        static bool down = true;
        static bool caseSensitive = true;
        char findText[128];
        snprintf(findText, sizeof(findText), "%s", doc->FindText.c_str());
        ImGui::SetNextItemWidth(220);
        if (ImGui::InputText("##find", findText, sizeof(findText)))
        {
            doc->FindText = findText;
        }
        ImGui::Checkbox("Case sensitive", &caseSensitive);
        ImGui::Checkbox("Down (search direction)", &down);
        if (ImGui::Button("Search next") || ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            doc->Status = TextEditor_Find(doc->FindText.c_str(), down, caseSensitive) ?
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

    // --- the Replace popup -------------------------------------------------
    if (ImGui::BeginPopup("Replace text"))
    {
        static bool caseSensitive = true;
        char findText[128];
        char replaceText[128];
        snprintf(findText, sizeof(findText), "%s", doc->FindText.c_str());
        snprintf(replaceText, sizeof(replaceText), "%s", doc->ReplaceText.c_str());
        ImGui::SetNextItemWidth(220);
        if (ImGui::InputText("Text", findText, sizeof(findText)))
        {
            doc->FindText = findText;
        }
        ImGui::SetNextItemWidth(220);
        if (ImGui::InputText("Replace with", replaceText, sizeof(replaceText)))
        {
            doc->ReplaceText = replaceText;
        }
        ImGui::Checkbox("Case sensitive", &caseSensitive);
        if (ImGui::Button("Search next"))
        {
            doc->Status = TextEditor_Find(doc->FindText.c_str(), true, caseSensitive) ?
                "Found" :
                "Not found";
        }
        ImGui::SameLine();
        if (ImGui::Button("Replace"))
        {
            TextEditor_Replace(
                doc->FindText.c_str(), doc->ReplaceText.c_str(), caseSensitive);
        }
        ImGui::SameLine();
        if (ImGui::Button("Replace all"))
        {
            doc->Status =
                TextEditor_ReplaceAll(doc->FindText.c_str(), doc->ReplaceText.c_str(),
                    caseSensitive) ?
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

    // --- the Go to line popup ----------------------------------------------
    if (ImGui::BeginPopup("Go to line"))
    {
        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Line index", &doc->GotoLine);
        if (ImGui::Button("Go") || ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            doc->GotoPending = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // --- the editor ---------------------------------------------------------
    // the widget ID is per document: two editors never share a line buffer
    char widgetId[600];
    snprintf(widgetId, sizeof(widgetId), "ScriptEditorText###%s",
        doc->Key.empty() ? doc->Name.c_str() : doc->Key.c_str());
    if (TextEditor_Draw(widgetId, &doc->Text, 0.0f, doc->GotoLine, &doc->GotoPending))
    {
        doc->Dirty = true;
    }

    // --- the statusbar -------------------------------------------------------
    ImGui::Separator();
    const vector<astr_t> functions = Script_AnalyzeFunctions(doc->Text);
    ImGui::Text("%s | %d lines | %d functions | %s", doc->Status.c_str(),
        (int)(std::count(doc->Text.begin(), doc->Text.end(), '\n') + 1),
        (int)functions.size(), doc->Path.empty() ? "(untitled)" : doc->Path.c_str());

    ImGui::End();
}

} // namespace xuoassist
