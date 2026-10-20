// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The script editor window body: one ImGui window per open document
// (ui/editors.cpp owns the document list and the save persistence). The
// toolbar keeps Save / Save As / Find / GoTo / Replace / Comment / Commands;
// Open is gone - files are opened by double-clicking a loaded script, and
// Save As goes through the native picker (ui/scriptpicker.cpp).

#include "../core/global.h"
#include "editors.h"

namespace xuoassist {

// Draws one document window; `open` is its visibility flag (the X button
// flips it; the document manager removes the document afterwards).
void ScriptEditorWindow_DrawDoc(CEditorDoc *doc, bool *open);

// Re-splits the document's editor widget after an external text replace
// (Reload from disk). `id` is the widget ID the window draws with.
void ScriptEditor_SyncDocument(const char *id, astr_t *buffer);

} // namespace xuoassist
