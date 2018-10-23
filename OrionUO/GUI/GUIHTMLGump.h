/***********************************************************************************
**
** GUIHTMLGump.h
**
** Компонента гампа с кнопками, скроллером прокрутки, фоном, служит контейнером для объектов
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef GUIHTMLGUMP_H
#define GUIHTMLGUMP_H

class CGUIHTMLGump : public CGUIPolygonal
{
public:
    //!Имеет фон
    bool HaveBackground = false;

    //!Имеет скроллер с кнопками
    bool HaveScrollbar = false;

    //!Размер данных, хранимых внутри компоненты
    Wisp::CSize DataSize = Wisp::CSize();

    //!Смещение к данным
    Wisp::CPoint2Di DataOffset = Wisp::CPoint2Di();

    //!Текущее смещение к данным
    Wisp::CPoint2Di CurrentOffset = Wisp::CPoint2Di();

    //!Доступное смещение для скроллинга
    Wisp::CPoint2Di AvailableOffset = Wisp::CPoint2Di();

private:
    void CalculateDataSize(CBaseGUI *item, int &startX, int &startY, int &endX, int &endY);

public:
    CGUIHTMLGump(
        int serial,
        ushort graphic,
        int x,
        int y,
        int width,
        int height,
        bool haveBackground,
        bool haveScrollbar);
    virtual ~CGUIHTMLGump();

    //!Ссылки на компоненты
    CGUIHTMLResizepic *m_Background{ NULL };
    CGUIHTMLButton *m_ButtonUp{ NULL };
    CGUIHTMLButton *m_ButtonDown{ NULL };
    CGUIHTMLHitBox *m_HitBoxLeft{ NULL };
    CGUIHTMLHitBox *m_HitBoxRight{ NULL };
    CGUIHTMLSlider *m_Slider{ NULL };
    CGUIScissor *m_Scissor{ NULL };

    //!Инициализация
    //!			menu - флаг инициализации для гампа CGumpMenu
    void Initalize(bool menu = false);

    //!Обновление высота гампа
    void UpdateHeight(int height);

    //!Сброс смещений
    void ResetDataOffset();

    //!Пересчет смещений
    void CalculateDataSize();

    virtual void PrepareTextures();

    virtual bool EntryPointerHere();

    virtual bool Select();

    //!Функция прокрутки
    virtual void Scroll(bool up, int delay);

    virtual bool IsHTMLGump() { return true; }
};

#endif
