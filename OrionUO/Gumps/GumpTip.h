// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGumpTip : public CGumpBaseScroll
{
    bool Updates = false;

private:
    static const int ID_GT_PREV_GUMP = 1;
    static const int ID_GT_REMOVE_GUMP = 2;
    static const int ID_GT_NEXT_GUMP = 3;

    CGUIButton *m_ButtonPrevGump{ nullptr };
    CGUIButton *m_ButtonNextGump{ nullptr };

    virtual void UpdateHeight();

public:
    CGumpTip(uint32_t serial, short x, short y, const string &str, bool updates);
    virtual ~CGumpTip();

    void SendTipRequest(uint8_t flag);

    GUMP_BUTTON_EVENT_H;

    virtual bool OnLeftMouseButtonDoubleClick();
};
