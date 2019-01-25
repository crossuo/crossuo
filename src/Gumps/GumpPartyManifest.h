// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpPartyManifest : public CGump
{
private:
    bool m_CanLoot = false;

public:
    CGumpPartyManifest(uint32_t serial, short x, short y, bool canLoot);
    virtual ~CGumpPartyManifest();

    virtual void UpdateContent() override;

    GUMP_BUTTON_EVENT_H;
};
