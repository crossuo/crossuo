// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../RenderObject.h"
#include "../Point.h"
#include "../plugin/enumlist.h"

class CTextData;

class CBaseGUI : public CRenderObject
{
public:
    //!Тип компоненты
    GUMP_OBJECT_TYPE Type = GOT_NONE;

    //!Возможность перемещения гампа, если компонента зажата
    bool MoveOnDrag = false;

    //!Флаг использования PartialHue
    bool PartialHue = false;

    //!Метод использования функции выбора
    //!			true - проверяет нахождение курсора мышки в пределах полигона (для CGUIPolygonal и компонент с шириной и высотой, либо берет габариты картинки исходного состояния Graphic)
    //!			false - стандартная функция проверки пикселей
    bool CheckPolygone = false;

    //!Включение выключение обработки компоненты (компонента отображается)
    bool Enabled = true;

    //!Показать/скрыть компоненту и ее обработку
    bool Visible = true;

    //!Флаг, отвечающий только за выбор компоненты, без ее отображения в гампе
    bool SelectOnly = false;

    //!Флаг, отвечающий только за отрисовку компоненты, без ее выбора
    bool DrawOnly = false;

    uint32_t ClilocID = 0;

    CTextData *TextData = nullptr;

    CBaseGUI(GUMP_OBJECT_TYPE type, int serial, uint16_t graphic, uint16_t color, int x, int y);
    virtual ~CBaseGUI();

    //!Выявление поля g_EntryPointer в данной компоненте
    virtual bool EntryPointerHere() { return false; }

    //!Установить данные для шейдера
    virtual void SetShaderMode() {}

    //!Получить ИД картинки для отображения
    virtual uint16_t GetDrawGraphic() { return Graphic; }

    //!Отрисовать компоненту
    //!		checktrans - использовать трафарет +прозрачность
    virtual void Draw(bool checktrans = false) {}

    //!Проверить компоненту на возможность выбора
    virtual bool Select() { return false; }

    //!Получить конечные габариты компоненты
    virtual CSize GetSize() { return CSize(); }

    //!Это компонента пользовательского интерфейса
    virtual bool IsGUI() { return true; }

    //!Это HTMLGump компонента
    virtual bool IsHTMLGump() { return false; }

    //!Это компонента HTMLGump'а (кнопки скроллинга, скроллер, фон, область вывода)
    virtual bool IsControlHTML() { return false; }

    //!Возможность обрабатывать события компонента, если она зажата, но мышка находится где-то еще
    virtual bool IsPressedOuthit() { return false; }
};
