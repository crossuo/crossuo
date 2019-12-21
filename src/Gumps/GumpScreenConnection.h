// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpScreenConnection : public CGump
{
private:
    static const int ID_CS_OK = 1;
    static const int ID_CS_CANCEL = 2;

    void CreateText(int x, int y, astr_t str, uint8_t font);

public:
    CGumpScreenConnection();
    virtual ~CGumpScreenConnection();
    virtual void UpdateContent() override;

    GUMP_BUTTON_EVENT_H override;
};
