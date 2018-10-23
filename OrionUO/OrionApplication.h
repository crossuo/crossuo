/***********************************************************************************
**
** OrionApplication.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef ORIONAPPLICATION_H
#define ORIONAPPLICATION_H

class COrionApplication : public Wisp::CApplication
{
public:
    uint NextRenderTime = 0;
    uint NextUpdateTime = 0;

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

#endif
