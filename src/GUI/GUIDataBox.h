// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "GUIPolygonal.h"

struct CGUIDataBox : public CGUIPolygonal
{
    CGUIDataBox();
    virtual ~CGUIDataBox() = default;
    virtual void PrepareTextures() override;
    virtual bool EntryPointerHere() override;
    virtual bool Select() override { return true; }
};
