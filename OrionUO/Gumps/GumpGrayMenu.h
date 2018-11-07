// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGumpGrayMenu : public CGump
{
public:
    CGumpGrayMenu(uint32_t serial, uint32_t id, short x, short y);
    virtual ~CGumpGrayMenu();

    static const int ID_GGM_CANCEL = 1000;
    static const int ID_GGM_CONTINUE = 1001;

    void SendMenuResponse(int index);

    GUMP_BUTTON_EVENT_H;
};
