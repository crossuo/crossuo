// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See editors.h: the document list, its lifecycle (open/focus/close/save) and
// the persistence side of Save / Save As. The window body (toolbar, widget,
// find/replace popups) is ScriptEditorWindow_DrawDoc in ui/scripteditor.cpp.

#include "editors.h"

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include <external/gfx/imgui/imgui.h>

#include "assistantui.h" // AssistantUI_MarkOptionsDirty
#include "editor.h"
#include "scripteditor.h"
#include "../script/scriptlist.h"

#include <stdio.h>

namespace xuoassist {

namespace {

vector<CEditorDoc *> s_Docs;
int s_UntitledSerial = 1;

astr_t next_untitled_name(astr_t baseName)
{
    if (baseName.empty())
    {
        baseName = "untitled";
    }
    char name[128];
    snprintf(name, sizeof(name), "%s-%d.js", baseName.c_str(), s_UntitledSerial++);
    return name;
}

void erase_doc(size_t index)
{
    TextEditor_ReleaseState(&s_Docs[index]->Text); // the astr_t is going away
    delete s_Docs[index];
    s_Docs.erase(s_Docs.begin() + index);
}

} // namespace

CEditorDoc *Editors_Find(const astr_t &pathKey)
{
    for (CEditorDoc *doc : s_Docs)
    {
        if (!doc->Key.empty() && doc->Key == pathKey)
        {
            return doc;
        }
    }
    return nullptr;
}

bool Editors_Open(const astr_t &path)
{
    const astr_t key = ScriptList_NormalizePath(path);
    if (CEditorDoc *existing = Editors_Find(key))
    {
        existing->Open = true;
        ImGui::SetWindowFocus(existing->Path.c_str());
        return true;
    }
    astr_t text;
    if (!ScriptList_ReadFile(path, text))
    {
        g_LoadedScripts.Add(path, 0u, false); // shows as [missing] in the tab
        return false;
    }
    // an opened file is a loaded script (it may have been opened through the
    // runtime's LoadScript, which imports first, or from the tab's row)
    g_LoadedScripts.Add(path, ScriptList_ContentHash(text));
    auto *doc = new CEditorDoc();
    doc->Path = path;
    doc->Key = key;
    doc->Name = ScriptList_DisplayName(path);
    doc->Text = text;
    doc->Status = "Opened " + path;
    s_Docs.push_back(doc);
    return true;
}

void Editors_OpenUntitled(const astr_t &initialText)
{
    Editors_OpenUntitled(astr_t(), initialText);
}

void Editors_OpenUntitled(const astr_t &baseName, const astr_t &initialText)
{
    auto *doc = new CEditorDoc();
    doc->Name = next_untitled_name(baseName);
    doc->Text = initialText;
    doc->Dirty = !initialText.empty();
    doc->Status = doc->Dirty ? "Unsaved changes" : "New script";
    s_Docs.push_back(doc);
}

void Editors_CloseAll()
{
    for (CEditorDoc *doc : s_Docs)
    {
        TextEditor_ReleaseState(&doc->Text);
        delete doc;
    }
    s_Docs.clear();
}

bool Editors_ReloadFromDisk(const astr_t &path)
{
    CEditorDoc *doc = Editors_Find(ScriptList_NormalizePath(path));
    if (doc == nullptr)
    {
        return false;
    }
    astr_t text;
    if (!ScriptList_ReadFile(path, text))
    {
        doc->Status = "Cannot open: " + path;
        return false;
    }
    doc->Text = text;
    doc->Dirty = false;
    doc->Status = "Reloaded " + path;
    Editors_ResyncWidget(doc);
    Editors_CommitSave(doc);
    return true;
}

void Editors_CommitSaveAs(CEditorDoc *doc, const astr_t &newPath)
{
    if (doc == nullptr)
    {
        return;
    }
    doc->Path = newPath;
    doc->Key = ScriptList_NormalizePath(newPath);
    doc->Name = ScriptList_DisplayName(newPath);
    doc->Dirty = false;
    doc->Status = "Saved as " + newPath;
    Editors_ResyncWidget(doc);
    // a Save As result is auto-loaded into the Scripts tab list
    g_LoadedScripts.Add(newPath, ScriptList_ContentHash(doc->Text));
    AssistantUI_MarkOptionsDirty();
}

void Editors_CommitSave(CEditorDoc *doc)
{
    if (doc == nullptr || doc->Key.empty())
    {
        return;
    }
    g_LoadedScripts.NotifySaved(doc->Key, doc->Text);
}

void Editors_ResyncWidget(CEditorDoc *doc)
{
    if (doc == nullptr || doc->Key.empty())
    {
        return;
    }
    ScriptEditor_SyncDocument(&doc->Text);
}

void Editors_Draw()
{
    for (size_t i = 0; i < s_Docs.size();)
    {
        CEditorDoc *doc = s_Docs[i];
        bool open = doc->Open;
        ScriptEditorWindow_DrawDoc(doc, &open);
        doc->Open = open;
        if (!doc->Open)
        {
            // the X button: an unsaved document asked first (the dialog is
            // drawn by the window), a clean one just goes away
            erase_doc(i);
            continue;
        }
        i++;
    }
}

} // namespace xuoassist
