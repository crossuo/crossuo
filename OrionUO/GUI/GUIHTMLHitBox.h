// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGUIHTMLHitBox : public CGUIHitBox
{
private:
    class CGUIHTMLGump *m_HTMLGump{ nullptr };

public:
    CGUIHTMLHitBox(
        class CGUIHTMLGump *htmlGump,
        int serial,
        int x,
        int y,
        int width,
        int height,
        bool callOnMouseUp = false);
    virtual ~CGUIHTMLHitBox();

    void Scroll(bool up, int delay);
    virtual bool IsControlHTML() { return true; }
};
