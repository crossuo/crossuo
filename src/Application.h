// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Wisp/WispApplication.h"

class CApplication : public Wisp::CApplication
{
protected:
    virtual void OnMainLoop();

public:
    CApplication()
        : Wisp::CApplication()
    {
    }
    virtual ~CApplication() {}
};

extern CApplication g_App;
