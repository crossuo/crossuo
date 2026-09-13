// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef XUOASSIST_UI_TABSCRIPTS_H
#define XUOASSIST_UI_TABSCRIPTS_H

// The Scripts tab (was TabScripts, assist/tabscripts.ui/.cpp, plan §2.7):
// the script file path row + browse/reload, the running-scripts list (one
// CScriptHandle per row: pause/terminate per row + the shared buttons), the
// running-actions start combo, sort checkbox, script-examples link and the
// editor opener.

namespace xuoassist {

void TabScripts_Draw();

} // namespace xuoassist

#endif // XUOASSIST_UI_TABSCRIPTS_H
