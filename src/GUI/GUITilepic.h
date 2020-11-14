// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include "GUIDrawObject.h"

struct CGUITilepic : public CGUIDrawObject
{
    CGUITilepic(uint16_t graphic, uint16_t color, int x, int y, bool doubleDraw = false);
    virtual ~CGUITilepic() = default;
    virtual CSize GetSize();
    virtual void PrepareTextures();
    virtual void Draw(bool checktrans = false);
    virtual bool Select();

private:
    bool DoubleDraw = false;
};
