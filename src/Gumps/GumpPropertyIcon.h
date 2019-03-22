// MIT License
// Copyright (C) November 2017 Hotride

#pragma once

#include "Gump.h"
#include "../Sprite.h"

class CGumpPropertyIcon : public CGump
{
protected:
    wstring m_Text = L"No Data";

private:
    const int ID_GPI_LOCK_MOVING = 1;
    const int ID_GPI_MINIMIZE = 2;

    CGLTextTexture m_Texture_DONT;
    CSprite m_Sprite;

public:
    CRenderObject *Object = nullptr;

    wstring GetTextW() { return m_Text; };
    void SetTextW(const wstring &val);

    virtual void PrepareContent();
    virtual void UpdateContent();
    GUMP_BUTTON_EVENT_H;
    virtual bool OnLeftMouseButtonDoubleClick();

    CGumpPropertyIcon(int x, int y);
    virtual ~CGumpPropertyIcon() = default;
};
