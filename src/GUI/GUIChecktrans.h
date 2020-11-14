// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include "GUIPolygonal.h"

class CGUIChecktrans : public CGUIPolygonal
{
public:
    CGUIChecktrans(int x, int y, int width, int height);
    virtual ~CGUIChecktrans();

    virtual void Draw(bool checktrans = false);
    virtual bool Select() { return false; }
};
