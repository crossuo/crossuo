// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The Object Inspector window (was assist/objectinspectorform.ui +
// assist/tabobjectinspector.cpp, 426x549 "Object Inspector").
//
// Layout delta (documented): Qt hosted one QTabWidget with a tab per
// inspected serial; here each inspected serial owns one collapsible section
// (the ImGui-native equivalent) - same per-serial snapshots, same tree.
//
// Tree content ports TabObjectInspector::Update (tabobjectinspector.cpp:
// 361-590): the "Game Object" group (Serial/Container/Top Object/Name/
// Graphic/Color/Count/X/Y/Z/Map), the "Flags" subgroup (Frozen/Poisoned/
// Yellow Hits/Locked/In War Mode/Hidden/Flying), "Ignored", the "Game Item"
// group (Layer; corpse suffix) or the "Game Character" group (Hits/Max Hits/
// Mana/Max Mana/Stam/Max Stam/Sex/Race/Direction/Running/Notoriety/Can
// Change Name) with the nested "Game Player" group (stats/locks/resists/...).
// A leaf click copies its value (on_tw_Info_itemClicked, :751-769 ->
// SetClipboardText).
//
// Stale serials: Qt froze the last snapshot when the object left the mirror
// (CheckUpdate bailed on FindWorldCharacter == nullptr); the section header
// here additionally carries an "(out of range)" marker.
//
// Delta: the item-art preview (TabObjectInspector::UpdateImage) is not
// reproduced - it decoded art directly from the file mapping; the client
// caches that art in its own render tree and there is no cheap in-process
// pixel handoff (plan §5.6 leaves art-to-ImGui a separate round).

#pragma once

#include "../core/global.h" // uint/typedefs

#if defined(XUO_ASSISTANT)

namespace xuoassist {

// The window body (assistantui.cpp draws it while the assistant is visible).
void InspectorWindow_Draw();

// ObjectInspectorForm::Update: shows the
// window, focuses the serial's section, (re)building its snapshot. Empty
// serials are ignored.
void Inspector_Open(uint serial);

// OnInspectorUpdate (was ObjectInspectorForm::CheckUpdate): refreshes the
// serial's snapshot from the mirror when the section exists.
void Inspector_OnInspectorUpdate(uint serial);

// Shutdown (context alive): drops the sections.
void Inspector_Shutdown();

} // namespace xuoassist

#endif // XUO_ASSISTANT
