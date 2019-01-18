// MIT License

#pragma once

#if defined(ORION_WINDOWS)

#include "StackWalker.h"
#include "Logging.h"

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
