// MIT License

#pragma once

#if defined(ORION_WINDOWS)

#include "StackWalker.h"
#include "Wisp/WispLogger.h"

class OrionStackWalker : public StackWalker
{
public:
    OrionStackWalker()
        : StackWalker()
    {
    }

protected:
    virtual void OnOutput(LPCSTR szText)
    {
        CRASHLOG(szText);
        StackWalker::OnOutput(szText);
    }
};

#endif
