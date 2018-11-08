// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGumpBulletinBoard : public CGump
{
private:
    //!Идентификаторы кнопок
    static const int ID_GBB_POST_MESSAGE = 1;
    static const int ID_GBB_MESSAGE = 2;
    static const int ID_GBS_HTMLGUMP = 3;

public:
    CGumpBulletinBoard(uint32_t serial, short x, short y, const string &name);
    virtual ~CGumpBulletinBoard();

    CGUIHTMLGump *m_HTMLGump{ nullptr };

    GUMP_BUTTON_EVENT_H;

    virtual bool OnLeftMouseButtonDoubleClick();
};
