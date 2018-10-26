/***********************************************************************************
**
** GumpMinimap.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef GUMPMINIMAP_H
#define GUMPMINIMAP_H

class CGumpMinimap : public CGump
{
public:
    ushort LastX = 0;
    ushort LastY = 0;

private:
    const int ID_GMM_LOCK_MOVING = 1;

    uchar m_Count{ 0 };

    CGLTexture m_Texture{ CGLTexture() };

    CGUIGumppic *m_Body{ nullptr };
    CGUIDataBox *m_DataBox{ nullptr };

    void GenerateMap();

    void CreatePixels(
        USHORT_LIST &data,
        int color,
        int x,
        int y,
        int width,
        int height,
        const Wisp::CPoint2Di *table,
        int count);

protected:
    virtual void CalculateGumpState();

public:
    CGumpMinimap(short x, short y, bool minimized);
    virtual ~CGumpMinimap();

    virtual void PrepareContent();

    virtual void UpdateContent();

    GUMP_BUTTON_EVENT_H;

    virtual bool OnLeftMouseButtonDoubleClick();
};

#endif
