// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef XUOASSIST_UI_SCRIPTMENU_H
#define XUOASSIST_UI_SCRIPTMENU_H

// The script editor command-insert menu (was the ~25-submenu context menu of
// CScriptPlainTextEdit::onCustomContextMenuRequested, gui/
// scriptplaintextedit.cpp:509-1522): the menu is DATA - a table of
// {menu path, label, insertion text} rows rendered as a nested popup, not
// hardcoded branches. Paths look like "Objects" or "Skills/SkillValue/real".

#include "../core/global.h"

namespace xuoassist {

struct XA_MENU_ITEM
{
    const char *Path;
    const char *Label;
    const char *Insertion;
};

// Renders the whole command menu as an ImGui popup under the last item;
// `inserted` receives the chosen insertion text (menu is modal-ish: the
// caller opens it with ImGui::OpenPopupOnItemClick or explicit OpenPopup).
// Returns true when the user picked an entry.
bool ScriptMenu_Draw(const char *popupId, string &inserted);

} // namespace xuoassist

#endif // XUOASSIST_UI_SCRIPTMENU_H
