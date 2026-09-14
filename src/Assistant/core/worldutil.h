// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// World-model helpers the assistant runs on the client's objects: the
// find/count engine's subtree walks, the equip-layer fallback and the
// find/ignore skip-mark reset. Declarations only; the definitions live in
// worldutil.cpp (the script harness declares these same functions as stubs
// instead of linking this file).

#pragma once

#include "global.h"
#include "../model/commonitems/finditem.h"
#include "../model/commonitems/ignoreitem.h"

class CGameObject;
class CGameItem;

namespace xuoassist {

// The layer an item occupies when equipped: the server-reported layer, or
// the client's used-layer table for items seen outside a paperdoll
int EquipLayer(const CGameItem *item);

// Sum of Count over the matching items in the object's subtree; the bank
// container is skipped when recursing. A 0xFFFF graphic/color matches any.
int CountItems(const CGameObject *root, const CFindItem &findListItem, bool recurse);
int CountItems(const CGameObject *root, ushort graphic, ushort color, bool recurse);

// First match in the object's subtree; matched serials append to list
CGameObject *FindType(
    const CGameObject *root,
    const CFindItem &findListItem,
    const vector<CIgnoreItem> &ignoreList,
    uchar flags,
    bool recurseSearch,
    vector<string> &list);

// Clear the find/ignore engine's skip marks on every object in the world
void IgnoreReset();

// The largest of the two axis deltas
int Distance(int x1, int y1, int x2, int y2);

} // namespace xuoassist
