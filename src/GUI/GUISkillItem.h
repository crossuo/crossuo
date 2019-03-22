// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"
#include "../Backend.h"

class CGUIButton;

class CGUISkillItem : public CBaseGUI
{
protected:
    uint8_t m_Status = 0;

private:
    CGLTextTexture m_NameText;
    CGLTextTexture m_ValueText;
    uint16_t GetStatusButtonGraphic();

public:
    int Index = 0;
    CGUIButton *m_ButtonUse = nullptr;
    CGUIButton *m_ButtonStatus = nullptr;

    uint8_t GetStatus() { return m_Status; };
    void SetStatus(uint8_t val);

    void CreateValueText(bool showReal = false, bool showCap = false);

    virtual CSize GetSize() { return CSize(255, 17); }
    virtual void PrepareTextures();
    virtual void Draw(bool checktrans = false);
    virtual bool Select();

    CBaseGUI *SelectedItem();

    CGUISkillItem(int serial, int useSerial, int statusSerial, int index, int x, int y);
    virtual ~CGUISkillItem();
};
