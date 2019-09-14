// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpBulletinBoard : public CGump
{
public:
    CGumpBulletinBoard(uint32_t serial, short x, short y, const std::string &name);
    virtual ~CGumpBulletinBoard();

    CGUIHTMLGump *m_HTMLGump{ nullptr };

    GUMP_BUTTON_EVENT_H override;

    virtual bool OnLeftMouseButtonDoubleClick() override;
};
