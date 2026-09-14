// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "worldutil.h"
#include "world.h"
#include "assist.h" // CAssistant::SerialToText (the find-result serial texts)
#include "CrossUO.h" // ::g_Game (the used-layer table)

namespace xuoassist {

int EquipLayer(const CGameItem *item)
{
    if (item == nullptr)
    {
        return 0;
    }

    int layer = item->Layer;

    if (layer == 0)
    {
        layer = g_Game.ValueInt(VKI_USED_LAYER, item->Graphic);
    }

    return layer;
}

int CountItems(const CGameObject *root, const CFindItem &findListItem, bool recurse)
{
    if (root == nullptr)
    {
        return 0;
    }

    int count = 0;

    for (CGameItem *item = (CGameItem *)root->m_Items; item != nullptr; item = (CGameItem *)item->m_Next)
    {
        if (findListItem.Found(item->Graphic, item->Color))
        {
            count += item->Count;
        }

        if (recurse && !item->Empty() && item->Layer != OL_BANK)
        {
            count += CountItems(item, findListItem, recurse);
        }
    }

    return count;
}

int CountItems(const CGameObject *root, ushort graphic, ushort color, bool recurse)
{
    if (root == nullptr)
    {
        return 0;
    }

    int count = 0;

    for (CGameItem *item = (CGameItem *)root->m_Items; item != nullptr; item = (CGameItem *)item->m_Next)
    {
        if ((graphic == 0xFFFF || graphic == item->Graphic) &&
            (color == 0xFFFF || color == item->Color))
        {
            count += item->Count;
        }

        if (recurse && !item->Empty() && item->Layer != OL_BANK)
        {
            count += CountItems(item, graphic, color, recurse);
        }
    }

    return count;
}

CGameObject *FindType(
    const CGameObject *root,
    const CFindItem &findListItem,
    const vector<CIgnoreItem> &ignoreList,
    uchar flags,
    bool recurseSearch,
    vector<string> &list)
{
    if (root == nullptr)
    {
        return nullptr;
    }

    CGameObject *result = nullptr;

    vector<CGameObject *> containers;

    for (CGameItem *item = (CGameItem *)root->m_Items; item != nullptr; item = (CGameItem *)item->m_Next)
    {
        if (!item->Ignored && findListItem.Found(item->Graphic, item->Color))
        {
            bool ignored = false;

            if (!ignoreList.empty())
            {
                for (const CIgnoreItem &ii : ignoreList)
                {
                    if (ii.Found(item->Serial, item->Graphic, item->Color))
                    {
                        ignored = true;
                        break;
                    }
                }
            }

            if (!ignored)
            {
                list.push_back(CAssistant::SerialToText(item->Serial));

                result = item;

                if ((flags & FTF_FAST) != 0)
                {
                    break;
                }
            }
        }

        if (recurseSearch && !item->Empty() && item->Layer != OL_BANK)
        {
            containers.push_back(item);
        }
    }

    if (((flags & FTF_FAST) == 0) || result == nullptr)
    {
        for (CGameObject *item : containers)
        {
            CGameObject *temp = FindType(item, findListItem, ignoreList, flags, recurseSearch, list);

            if (temp != nullptr)
            {
                result = temp;

                if ((flags & FTF_FAST) != 0)
                {
                    break;
                }
            }
        }
    }

    return result;
}

void IgnoreReset()
{
    if (g_World == nullptr)
    {
        return;
    }

    for (WORLD_MAP::iterator i = g_World->m_Map.begin(); i != g_World->m_Map.end(); ++i)
    {
        i->second->Ignored = false;
    }
}

int Distance(int x1, int y1, int x2, int y2)
{
    int distx = abs(x2 - x1);
    int disty = abs(y2 - y1);

    return distx > disty ? distx : disty;
}

} // namespace xuoassist
