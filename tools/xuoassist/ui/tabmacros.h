// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// TabMacros (Phase 3, master plan §2.6/§7 item 3): the macro list, the actions
// list of the selected macro, New/Delete/Record/Play, the loop controls and
// the disabled To-script placeholder. Was assist/tabmacros.{ui,cpp}; the
// playback engine is macros/macroexecutor.cpp (the frame tick).

#ifndef XUOASSIST_UI_TABMACROS_H
#define XUOASSIST_UI_TABMACROS_H

namespace xuoassist {

void TabMacros_Draw();

} // namespace xuoassist

#endif // XUOASSIST_UI_TABMACROS_H
