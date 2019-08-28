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
    CTextSprite m_Sprite;

public:
    CRenderObject *Object = nullptr;

    wstring GetTextW() { return m_Text; };
    void SetTextW(const wstring &val);

    virtual void PrepareContent() override;
    virtual void UpdateContent() override;
    GUMP_BUTTON_EVENT_H override;
    virtual bool OnLeftMouseButtonDoubleClick() override;

    CGumpPropertyIcon(int x, int y);
    virtual ~CGumpPropertyIcon() = default;
};
