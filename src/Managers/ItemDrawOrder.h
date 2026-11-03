// SPDX-License-Identifier: BSD-2-Clause

// Draw order is keyed on the equipped items' AnimIDs, not just their layer:
//   1. copy one of THREE base order tables (chosen by arms/torso body),
//   2. apply per-graphic reorder rules (swap / move-to / move-after),
//   3. paint in that order - later index = painted on top.
//
// The tables drive both the paperdoll gump and the in-world equipment views.
// Table ORDER_TABLE_DEFAULT is the default; ORDER_TABLE_ARMS_LATE draws arms late; ORDER_TABLE_TORSO_FRONT pulls the torso
// to the front (chest-under variant for female/gargoyle bodies).

#ifndef ITEM_DRAW_ORDER_H
#define ITEM_DRAW_ORDER_H

#include <cstdint>

class CGameObject;

namespace ItemDrawOrder
{
// 25 entries: leading invalid sentinel + the 24 equippable layers.
constexpr int LAYER_SLOTS = 0x19;

// Compute the back-to-front paint order into `order` (size >= LAYER_SLOTS) from the
// per-layer AnimIDs in `graphics` (indexed by layer value, 0 = empty slot).
void ComputeDrawOrder(const uint16_t *graphics, bool altTorsoTable, int *order);

// Fill `graphics` (size >= LAYER_SLOTS) with obj's equipped per-layer AnimIDs.
void CollectEquippedAnimIDs(const CGameObject *obj, uint16_t *graphics);

// Copy a ComputeDrawOrder() result into `dest`, dropping the sentinel and
// optionally the backpack. Returns the entry count.
int CollapseOrder(const int *order, bool includeBackpack, int *dest);

// One-shot for the in-world (animated) equipment views: compute the order,
// drop the skipped slots and reposition the cloak by facing direction. Returns the entry count written to `dest`.
int ResolveWorldOrder(const CGameObject *obj, uint8_t direction, int *dest);

// The official draw order repositions the cloak on facing direction:
// dir 0 (away from the viewer) paints the cloak last (on top), dir 3 (toward
// the viewer) paints it first (behind the body), every other direction places
// it just below the helmet. Operates in place; returns the (unchanged) count.
int PlaceCloakByFacing(int *layers, int count, uint8_t dir);

// true for female/gargoyle bodies - selects the alternate torso base table.
bool UsesFrontTorsoLayout(const CGameObject *obj);

// true when the equipped robe is a hooded shroud (or similar head-covering
// robe) that hides the hair/helmet on the paperdoll. Keyed on the robe item's
// Graphic (not the AnimID), covering hair and helmet, with helmets of the
// 0xA42B/0xA42C graphics hiding the hair as well.
bool IsHeadHiddenByRobe(const CGameObject *obj, int layer);
}; // namespace ItemDrawOrder

#endif // ITEM_DRAW_ORDER_H
