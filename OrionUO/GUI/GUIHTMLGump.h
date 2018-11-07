// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGUIHTMLGump : public CGUIPolygonal
{
public:
    bool HaveBackground = false;
    bool HaveScrollbar = false;
    Wisp::CSize DataSize = Wisp::CSize();
    Wisp::CPoint2Di DataOffset = Wisp::CPoint2Di();
    Wisp::CPoint2Di CurrentOffset = Wisp::CPoint2Di();
    Wisp::CPoint2Di AvailableOffset = Wisp::CPoint2Di();

private:
    void CalculateDataSize(CBaseGUI *item, int &startX, int &startY, int &endX, int &endY);

public:
    CGUIHTMLGump(
        int serial,
        uint16_t graphic,
        int x,
        int y,
        int width,
        int height,
        bool haveBackground,
        bool haveScrollbar);
    virtual ~CGUIHTMLGump();

    CGUIHTMLResizepic *m_Background{ nullptr };
    CGUIHTMLButton *m_ButtonUp{ nullptr };
    CGUIHTMLButton *m_ButtonDown{ nullptr };
    CGUIHTMLHitBox *m_HitBoxLeft{ nullptr };
    CGUIHTMLHitBox *m_HitBoxRight{ nullptr };
    CGUIHTMLSlider *m_Slider{ nullptr };
    CGUIScissor *m_Scissor{ nullptr };

    void Initalize(bool menu = false);
    void UpdateHeight(int height);
    void ResetDataOffset();
    void CalculateDataSize();
    virtual void PrepareTextures();
    virtual bool EntryPointerHere();
    virtual bool Select();
    virtual void Scroll(bool up, int delay);
    virtual bool IsHTMLGump() { return true; }
};
