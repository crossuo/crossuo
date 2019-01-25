// MIT License

#pragma once

#if defined(XUO_WINDOWS)

#include "StackWalker.h"
#include "Logging.h"

class GameStackWalker : public StackWalker
{
public:
    GameStackWalker()
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
