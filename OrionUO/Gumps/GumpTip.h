// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGumpTip : public CGumpBaseScroll
{
    bool Updates = false;
    CGUIButton *m_ButtonPrevGump{ nullptr };
    CGUIButton *m_ButtonNextGump{ nullptr };

    virtual void UpdateHeight();

public:
    CGumpTip(uint32_t serial, short x, short y, const string &str, bool updates);
    virtual ~CGumpTip();

    void SendTipRequest(uint8_t flag);

    GUMP_BUTTON_EVENT_H;

    virtual bool OnLeftMouseButtonDoubleClick() override;
};
