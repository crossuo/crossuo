// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGumpScreenCharacterList : public CGump
{
public:
    CGumpScreenCharacterList();
    virtual ~CGumpScreenCharacterList();

    virtual void UpdateContent();
    virtual void InitToolTip();

    GUMP_BUTTON_EVENT_H;
    GUMP_TEXT_ENTRY_EVENT_H;

    virtual bool OnLeftMouseButtonDoubleClick() override;
};
