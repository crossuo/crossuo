// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGumpScreenCharacterList : public CGump
{
private:
    static const int ID_CS_QUIT = 1;
    static const int ID_CS_ARROW_PREV = 2;
    static const int ID_CS_ARROW_NEXT = 3;
    static const int ID_CS_NEW = 4;
    static const int ID_CS_DELETE = 5;
    static const int ID_CS_CHARACTERS = 6;

public:
    CGumpScreenCharacterList();
    virtual ~CGumpScreenCharacterList();

    virtual void UpdateContent();
    virtual void InitToolTip();

    GUMP_BUTTON_EVENT_H;
    GUMP_TEXT_ENTRY_EVENT_H;

    virtual bool OnLeftMouseButtonDoubleClick();
};
