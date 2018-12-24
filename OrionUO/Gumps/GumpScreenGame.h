// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGumpScreenGame : public CGump
{
private:
    static const int ID_GS_RESIZE = 1;

public:
    CGumpScreenGame();
    virtual ~CGumpScreenGame();

    virtual void UpdateContent() override;
    virtual void InitToolTip() override;
    virtual void Draw() override;
    virtual CRenderObject *Select() override;
    virtual void OnLeftMouseButtonDown() override;
    virtual void OnLeftMouseButtonUp() override;
};
