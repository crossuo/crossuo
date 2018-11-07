// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CClickObject
{
public:
    uint32_t Timer = 0;
    uint16_t X = 0;
    uint16_t Y = 0;
    bool Enabled = false;
    uint32_t Page = 0;
    CRenderObject *Object = nullptr;
    CGump *Gump = nullptr;
    CClickObject() {}
    ~CClickObject() {}

    void Clear(CRenderObject *obj);
    void Clear();

    void Init(CRenderObject *obj, CGump *gump = nullptr);
};

extern CClickObject g_ClickObject; //Ссылка на объект клика
