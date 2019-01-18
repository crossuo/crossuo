// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUIShopResult : public CBaseGUI
{
public:
    //!Стоимость
    uint32_t Price = 0;

    //!Название
    string Name = "";

private:
    //!Текстура названия
    CGLTextTexture m_NameText{ CGLTextTexture() };

public:
    CGUIShopResult(class CGUIShopItem *shopItem, int x, int y);
    virtual ~CGUIShopResult();

    //!Компонента кнопок MinMax
    class CGUIMinMaxButtons *m_MinMaxButtons{ nullptr };

    //!Получить ссылку на выбранную компоненту или ссылку на себя
    CBaseGUI *SelectedItem();

    virtual CSize GetSize() { return CSize(200, m_NameText.Height); }

    virtual void PrepareTextures();

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};
