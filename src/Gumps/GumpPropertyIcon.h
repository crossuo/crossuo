// MIT License
// Copyright (C) November 2017 Hotride

#pragma once

#include "Gump.h"

class CGumpPropertyIcon : public CGump
{
protected:
    wstring m_Text = L"No Data";

public:
    wstring GetTextW() { return m_Text; };
    void SetTextW(const wstring &val);
    CRenderObject *Object = nullptr;

private:
    const int ID_GPI_LOCK_MOVING = 1;
    const int ID_GPI_MINIMIZE = 2;

    CGLTextTexture m_Texture;

public:
    CGumpPropertyIcon(int x, int y);
    virtual ~CGumpPropertyIcon();

    virtual void PrepareContent();

    virtual void UpdateContent();

    GUMP_BUTTON_EVENT_H;

    virtual bool OnLeftMouseButtonDoubleClick();
};
