// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class COrionApplication : public Wisp::CApplication
{
public:
    uint32_t NextRenderTime = 0;
    uint32_t NextUpdateTime = 0;

protected:
    virtual void OnMainLoop();

public:
    COrionApplication()
        : Wisp::CApplication()
    {
    }
    virtual ~COrionApplication() {}
};

extern COrionApplication g_App;
