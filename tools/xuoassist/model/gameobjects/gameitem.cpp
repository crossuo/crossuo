// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (gameobjects/gameitem.cpp), verbatim.

#include "gameitem.h"

namespace xuoassist {

CGameItem::CGameItem(const uint &serial)
    : CGameObject(serial)
{
}

CGameItem::~CGameItem()
{
}

int CGameItem::GetEquipLayer()
{
    int layer = m_Layer;

    if (layer == 0)
    {
        layer = GetInt(VKI_USED_LAYER, m_Graphic);
    }

    return layer;
}

CGameItem *CGameItem::FindItem(const ushort &graphic, const ushort &color)
{
    CGameItem *item = nullptr;

    if (color == 0xFFFF) // Поиск по минимальному цвету
    {
        uint16_t minColor = 0xFFFF;

        QFOR(obj, m_Items, CGameItem *)
        {
            if (obj->GetGraphic() == graphic)
            {
                if (obj->GetColor() < minColor)
                {
                    item = obj;
                    minColor = obj->GetColor();
                }
            }

            if (!obj->Empty())
            {
                CGameItem *found = obj->FindItem(graphic, color);

                if (found != nullptr && found->GetColor() < minColor)
                {
                    item = found;
                    minColor = found->GetColor();
                }
            }
        }
    }
    else // стандартный поиск
    {
        QFOR(obj, m_Items, CGameItem *)
        {
            if (obj->GetGraphic() == graphic && obj->GetColor() == color)
            {
                item = obj;
            }

            if (!obj->Empty())
            {
                CGameItem *found = obj->FindItem(graphic, color);

                if (found != nullptr)
                {
                    item = found;
                }
            }
        }
    }

    return item;
}

} // namespace xuoassist
