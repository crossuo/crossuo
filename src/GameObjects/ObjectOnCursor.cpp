// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "ObjectOnCursor.h"
#include "GameObject.h"
#include "Globals.h" // IsStackable
#include <xuocore/mulstruct.h>

CObjectOnCursor g_ObjectInHand;

void CObjectOnCursor::Clear()
{
    Enabled = false;
    Dropped = false;
    Serial = 0;
    Graphic = 0;
    Color = 0;
    Count = 0;
    TotalCount = 0;
    Layer = 0;
    Flags = 0;
    Container = 0;
    IsGameFigure = false;
    TiledataPtr = nullptr;
    UpdatedInWorld = false;
}

uint16_t CObjectOnCursor::GetDrawGraphic(bool &doubleDraw)
{
    return get_stack_graphic(Graphic, Count, doubleDraw);
}
