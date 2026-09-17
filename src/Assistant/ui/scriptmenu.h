// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The script editor command-insert menu: the menu is DATA - a table of
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
bool ScriptMenu_Draw(const char *popupId, astr_t &inserted);

} // namespace xuoassist
