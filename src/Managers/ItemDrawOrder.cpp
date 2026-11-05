// SPDX-License-Identifier: BSD-2-Clause

// See ItemDrawOrder.h. Layer values come from xuocore/enumlist.h OL_*.

#include "ItemDrawOrder.h"

#include "../GameObjects/GameCharacter.h"
#include "../GameObjects/GameItem.h"
#include "../GameObjects/GameObject.h"

namespace
{
// Base tables, 25 entries each (leading OL_NONE is a sentinel). Back-to-front
// paint order - later index = painted on top.

// arms drawn late
const int ORDER_TABLE_ARMS_LATE[ItemDrawOrder::LAYER_SLOTS] = {
    OL_NONE,  OL_CLOAK,   OL_SHIRT,  OL_PANTS,    OL_SHOES,   OL_LEGS,    OL_TORSO,
    OL_TUNIC, OL_RING,    OL_BRACELET, OL_15,     OL_ARMS,    OL_GLOVES,  OL_SKIRT,
    OL_ROBE,  OL_WAIST,   OL_NECKLACE, OL_HAIR,   OL_BEARD,   OL_EARRINGS, OL_HELMET,
    OL_1_HAND, OL_2_HAND, OL_BACKPACK, OL_TALISMAN
};

// default - arms early
const int ORDER_TABLE_DEFAULT[ItemDrawOrder::LAYER_SLOTS] = {
    OL_NONE,  OL_CLOAK,   OL_SHIRT,  OL_PANTS,    OL_SHOES,   OL_LEGS,    OL_ARMS,
    OL_TORSO, OL_TUNIC,   OL_RING,   OL_BRACELET, OL_15,      OL_GLOVES,  OL_SKIRT,
    OL_ROBE,  OL_WAIST,   OL_NECKLACE, OL_HAIR,   OL_BEARD,   OL_EARRINGS, OL_HELMET,
    OL_1_HAND, OL_2_HAND, OL_BACKPACK, OL_TALISMAN
};

// torso pulled to the front (chest-under variant)
const int ORDER_TABLE_TORSO_FRONT[ItemDrawOrder::LAYER_SLOTS] = {
    OL_NONE,  OL_CLOAK,   OL_TORSO,  OL_SHIRT,    OL_PANTS,   OL_SHOES,   OL_LEGS,
    OL_TUNIC, OL_RING,    OL_BRACELET, OL_15,     OL_ARMS,    OL_GLOVES,  OL_SKIRT,
    OL_ROBE,  OL_WAIST,   OL_NECKLACE, OL_HAIR,   OL_BEARD,   OL_EARRINGS, OL_HELMET,
    OL_1_HAND, OL_2_HAND, OL_BACKPACK, OL_TALISMAN
};

int IndexOf(const int *order, int value)
{
    for (int i = 0; i < ItemDrawOrder::LAYER_SLOTS; i++)
    {
        if (order[i] == value)
        {
            return i;
        }
    }
    return -1;
}

// Relocate value A so it lands on B's index (exact memmove semantics).
void MoveTo(int *order, int a, int b)
{
    const int i1 = IndexOf(order, a);
    if (i1 < 0)
    {
        return;
    }
    const int i2 = IndexOf(order, b);
    if (i2 < 0 || i2 == i1)
    {
        return;
    }
    if (i2 < i1)
    {
        for (int k = i1; k > i2; k--)
        {
            order[k] = order[k - 1];
        }
    }
    else
    {
        for (int k = i1; k < i2; k++)
        {
            order[k] = order[k + 1];
        }
    }
    order[i2] = a;
}

// Relocate value A so it lands immediately AFTER B.
void MoveAfter(int *order, int a, int b)
{
    const int i1 = IndexOf(order, a);
    if (i1 < 0)
    {
        return;
    }
    const int i2 = IndexOf(order, b);
    if (i2 < 0)
    {
        return;
    }
    int value = order[i1];
    if (i1 < i2)
    {
        for (int k = i1; k < i2; k++)
        {
            order[k] = order[k + 1];
        }
        order[i2] = value;
    }
    else
    {
        for (int k = i1; k > i2 + 1; k--)
        {
            order[k] = order[k - 1];
        }
        order[i2 + 1] = value;
    }
}
} // namespace

namespace ItemDrawOrder
{
void ComputeDrawOrder(const uint16_t *graphics, bool altTorsoTable, int *order)
{
    // 1) base table selection. An arms-graphic alternate match locks the
    // arms-late table and skips the torso/race checks entirely. The rules are
    // keyed on the AnimID (the gump/equip display id), not the world tile
    // graphic - e.g. bone-armor arms 0x1410/0x1417 both resolve to 0x210.
    const int *table = ORDER_TABLE_DEFAULT;

    const uint16_t arms = graphics[OL_ARMS];
    bool armsAlternate;
    if (arms < 0x3d0)
    {
        armsAlternate = (arms == 0x3cf || arms == 0x210 || arms == 0x3b3);
    }
    else
    {
        armsAlternate = (arms == 0x3dd);
    }

    if (armsAlternate)
    {
        table = ORDER_TABLE_ARMS_LATE;
    }
    else
    {
        const uint16_t torso = graphics[OL_TORSO];
        if (torso == 0x21a)
        {
            table = ORDER_TABLE_ARMS_LATE;
        }
        else if (torso - 0x399 < 5 && altTorsoTable)
        {
            table = ORDER_TABLE_TORSO_FRONT;
        }
    }

    for (int i = 0; i < LAYER_SLOTS; i++)
    {
        order[i] = table[i];
    }

    // 2) per-graphic reorder rules.
    // shirt present + pants gfx 0x398 -> pants at the shirt's slot
    if (graphics[OL_SHIRT] != 0 && graphics[OL_PANTS] == 0x398)
    {
        MoveTo(order, OL_PANTS, OL_SHIRT);
    }

    // keep shoes under pants for the skirt-like pants graphics
    const uint16_t pants = graphics[OL_PANTS];
    bool skipFinalPantsCheck = false;
    if (pants < 0x201)
    {
        if (pants == 0x200 || pants == 0x1eb || pants == 0x1fa)
        {
            const int iShoes = IndexOf(order, OL_SHOES);
            const int iPants = IndexOf(order, OL_PANTS);
            if (iShoes >= 0 && iPants >= 0 && iPants < iShoes)
            {
                order[iShoes] = OL_PANTS;
                order[iPants] = OL_SHOES;
            }
        }
    }
    else if (pants - 0x513 < 2)
    {
        if (graphics[OL_SHOES] != 0)
        {
            MoveTo(order, OL_PANTS, OL_SHOES);
        }
        skipFinalPantsCheck = true;
    }

    if (!skipFinalPantsCheck && graphics[OL_SHOES] != 0 && graphics[OL_PANTS] == 0x3e4)
    {
        MoveTo(order, OL_PANTS, OL_SHOES);
    }

    // tunic gfx 0x238 -> tunic over the belt; with one of the special robes,
    // the robe moves to the necklace slot (painted over the chest)
    if (graphics[OL_TUNIC] == 0x238)
    {
        MoveAfter(order, OL_TUNIC, OL_WAIST);
        if (graphics[OL_ROBE] != 0)
        {
            const uint16_t r = graphics[OL_ROBE];
            if (r == 0x4e8 || r == 0x4e9 || r == 0x4ea || r == 0x4eb || r == 0x5e2 ||
                r == 0x5e3 || r == 0x5e4 || r == 0x5e5)
            {
                MoveTo(order, OL_ROBE, OL_NECKLACE);
            }
        }
    }

    // QUIVER FIX: quivers are worn on the cloak layer; the original client
    // draws their graphics (0x380/0x5F3) over the chest - move after the robe.
    const uint16_t cloak = graphics[OL_CLOAK];
    if (cloak == 0x380 || cloak == 0x5f3)
    {
        MoveAfter(order, OL_CLOAK, OL_ROBE);
    }

    // helmet/neck interplay
    const uint16_t helm = graphics[OL_HELMET];
    if (helm < 0x202)
    {
        const uint16_t neck = graphics[OL_NECKLACE];
        if ((helm == 0x201 || helm == 0x1a9) &&
            (neck == 0x1c8 || (neck > 0x1d6 && neck < 0x1d9)))
        {
            MoveAfter(order, OL_NECKLACE, OL_HELMET);
            return; // done - the helmet covers the neck item
        }
    }
    else if (helm - 0x5e9 < 2 && graphics[OL_ROBE] != 0 &&
             (graphics[OL_ROBE] - 0x5e2) < 4)
    {
        MoveTo(order, OL_ROBE, OL_HELMET);
    }
}

void CollectEquippedAnimIDs(const CGameObject *obj, uint16_t *graphics)
{
    for (int layer = 0; layer < LAYER_SLOTS; layer++)
    {
        graphics[layer] = 0;
    }

    for (int layer = OL_1_HAND; layer <= OL_LEGS; layer++)
    {
        const CGameItem *item = obj->FindLayer(layer);
        if (item != nullptr)
        {
            graphics[layer] = item->AnimID;
        }
    }
}

int CollapseOrder(const int *order, bool includeBackpack, int *dest)
{
    int count = 0;
    for (int i = 0; i < LAYER_SLOTS; i++)
    {
        const int layer = order[i];
        if (layer == OL_NONE || layer == OL_MOUNT)
        {
            continue;
        }
        if (layer == OL_BACKPACK && !includeBackpack)
        {
            continue;
        }
        dest[count++] = layer;
    }
    return count;
}

int PlaceCloakByFacing(int *layers, int count, uint8_t dir)
{
    int idx = -1;
    for (int i = 0; i < count; i++)
    {
        if (layers[i] == OL_CLOAK)
        {
            idx = i;
            break;
        }
    }

    if (idx < 0)
    {
        return count;
    }

    // remove the cloak
    for (int i = idx; i < count - 1; i++)
    {
        layers[i] = layers[i + 1];
    }
    count--;

    int insert;
    if (dir == 0)
    {
        insert = count; // painted last = on top
    }
    else if (dir == 3)
    {
        insert = 0; // painted first = behind the body
    }
    else
    {
        insert = count;
        for (int i = 0; i < count; i++)
        {
            if (layers[i] == OL_HELMET)
            {
                insert = i;
                break;
            }
        }
    }

    // open a slot at `insert` and drop the cloak in
    for (int i = count; i > insert; i--)
    {
        layers[i] = layers[i - 1];
    }
    layers[insert] = OL_CLOAK;
    return count + 1;
}

int ResolveWorldOrder(const CGameObject *obj, uint8_t direction, int *dest)
{
    uint16_t graphics[LAYER_SLOTS];
    CollectEquippedAnimIDs(obj, graphics);

    int order[LAYER_SLOTS];
    ComputeDrawOrder(graphics, UsesFrontTorsoLayout(obj), order);

    const int count = CollapseOrder(order, false, dest);
    // the facing rule is for real cloaks (they hang behind the body). A quiver
    // shares the cloak slot but is worn on the back - pushed behind the body
    // sprite it would vanish when facing the viewer, so it keeps its fixed
    // over-the-robe position in every direction.
    const uint16_t cloak = graphics[OL_CLOAK];
    if (cloak == 0x380 || cloak == 0x5f3)
    {
        return count;
    }
    return PlaceCloakByFacing(dest, count, direction);
}

bool UsesFrontTorsoLayout(const CGameObject *obj)
{
    uint16_t body = obj->Graphic;
    if (obj->IsCorpse())
    {
        body = ((const CGameItem *)obj)->Count;
    }

    switch (body)
    {
        case 0x029A: // gargoyles
        case 0x029B:
        case 0x02B6:
        case 0x02B7:
        case 0x0666:
        case 0x0667:
            return true;
        case 0x0191: // human female
        case 0x025D: // elf female
            return true;
        default:
            break;
    }

    if (obj->NPC && ((const CGameCharacter *)obj)->Gender == GENDER_FEMALE)
    {
        return true;
    }
    return false;
}

bool IsHeadHiddenByRobe(const CGameObject *obj, int layer)
{
    if (layer != OL_HAIR && layer != OL_HELMET)
    {
        return false;
    }

    // a 0xA42B/0xA42C helmet hides the hair as well
    if (layer == OL_HAIR)
    {
        const CGameItem *helm = obj->FindLayer(OL_HELMET);
        if (helm != nullptr && (uint16_t)(helm->Graphic - 0xA42B) < 2)
        {
            return true;
        }
    }

    const CGameItem *robe = obj->FindLayer(OL_ROBE);
    if (robe == nullptr)
    {
        return false;
    }

    // hood-style robes hide both hair and helmet; keyed on the robe's item
    // Graphic
    const uint16_t robeGfx = robe->Graphic;
    bool covered;
    if (robeGfx < 0x4B9E)
    {
        if (robeGfx != 0x4B9D)
        {
            if (robeGfx < 0x2FBA)
            {
                if (robeGfx != 0x2FB9)
                {
                    if (robeGfx > 0x2687)
                    {
                        return false;
                    }
                    if (robeGfx < 0x2683 && (robeGfx < 0x204E || robeGfx > 0x204F))
                    {
                        return false;
                    }
                }
            }
            else if (robeGfx != 0x3173)
            {
                return false;
            }
            covered = true;
        }
        else
        {
            covered = true;
        }
    }
    else if (robeGfx < 0xA0B0)
    {
        covered = (robeGfx >= 0xA0AB || robeGfx == 0x7816);
    }
    else
    {
        covered = (robeGfx == 0xB2B7);
    }

    if (!covered)
    {
        return false;
    }

    // these hoods cover the head on every body except gargoyles
    uint16_t body = obj->Graphic;
    if (obj->IsCorpse())
    {
        body = ((const CGameItem *)obj)->Count;
    }
    const bool isGargoyle =
        body == 0x029A || body == 0x029B || body == 0x02B6 || body == 0x02B7;
    return !isGargoyle;
}
}; // namespace ItemDrawOrder
