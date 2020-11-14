// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <stdint.h>

class CGump;
class CRenderObject;

class CSelectedObject
{
public:
    uint32_t Serial = 0;
    CRenderObject *Object = nullptr;
    CGump *Gump = nullptr;
    CSelectedObject();
    virtual ~CSelectedObject();

    void Clear();
    void Clear(CRenderObject *obj);

    void Init(CRenderObject *obj, CGump *gump = nullptr);
    void Init(const CSelectedObject &obj);
};

extern CSelectedObject g_SelectedObject;
extern CSelectedObject g_LastSelectedObject;
