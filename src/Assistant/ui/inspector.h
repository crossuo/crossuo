// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The Object Inspector window: one collapsible section per inspected serial
// (same per-serial snapshots, same tree).
// Tree content: the "Game Object" group (Serial/Container/Top Object/Name/
// Graphic/Color/Count/X/Y/Z/Map), the "Flags" subgroup (Frozen/Poisoned/
// Yellow Hits/Locked/In War Mode/Hidden/Flying), "Ignored", the "Game Item"
// group (Layer; corpse suffix) or the "Game Character" group (Hits/Max Hits/
// Mana/Max Mana/Stam/Max Stam/Sex/Race/Direction/Running/Notoriety/Can
// Change Name) with the nested "Game Player" group (stats/locks/resists/...).
// A leaf click copies its value to the clipboard.
// Stale serials: the last snapshot freezes when the object leaves the mirror;
// the section header additionally carries an "(out of range)" marker.

#pragma once

#include "../core/global.h"

#if defined(XUO_ASSISTANT)

namespace xuoassist {

// The window body (assistantui.cpp draws it while the assistant is visible).
void InspectorWindow_Draw();

// ObjectInspectorForm::Update: shows the
// window, focuses the serial's section, (re)building its snapshot. Empty
// serials are ignored.
void Inspector_Open(uint32_t serial);

// OnInspectorUpdate: refreshes the
void Inspector_OnInspectorUpdate(uint32_t serial);

// Shutdown (context alive): drops the sections.
void Inspector_Shutdown();

} // namespace xuoassist

#endif // XUO_ASSISTANT
