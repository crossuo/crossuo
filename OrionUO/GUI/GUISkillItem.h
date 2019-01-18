// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"
#include "../GLEngine/GLTextTexture.h"
class CGUIButton;

class CGUISkillItem : public CBaseGUI
{
public:
    //!Индекс навыка
    int Index = 0;

    //!Статус навыка
protected:
    uint8_t m_Status = 0;

public:
    uint8_t GetStatus() { return m_Status; };
    void SetStatus(uint8_t val);

private:
    //!Текустуры текста
    CGLTextTexture m_NameText{ CGLTextTexture() };
    CGLTextTexture m_ValueText{ CGLTextTexture() };

    //!Получить ИД картинки кнопки статуса
    uint16_t GetStatusButtonGraphic();

public:
    CGUISkillItem(int serial, int useSerial, int statusSerial, int index, int x, int y);
    virtual ~CGUISkillItem();

    //!Компоненты управления
    CGUIButton *m_ButtonUse{ nullptr };
    CGUIButton *m_ButtonStatus{ nullptr };

    //!Создать текстуру значения навыка
    void CreateValueText(bool showReal = false, bool showCap = false);

    virtual CSize GetSize() { return CSize(255, 17); }

    virtual void PrepareTextures();

    virtual void Draw(bool checktrans = false);
    virtual bool Select();

    //!Получить ссылку на выбранную компоненту или на себя
    CBaseGUI *SelectedItem();
};
