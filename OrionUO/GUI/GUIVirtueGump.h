// MIT License
// Copyright (C) May 2017 Hotride

#pragma once

class CGUIVirtureGump : public CGUIDrawObject
{
public:
    CGUIVirtureGump(uint16_t graphic, int x, int y);
    virtual ~CGUIVirtureGump();
    virtual bool Select();
};
