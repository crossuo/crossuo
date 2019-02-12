// MIT License

#include "WispThread.h"
#include <SDL_timer.h>
#include "../FileSystem.h"
#include "../GameWindow.h"

namespace Wisp
{
CApplication::CApplication()
{
}

void CApplication::Init()
{
    Info(Client, "initializing application");
    g_MainThread = CThread::GetCurrentThreadId();
    DEBUG_TRACE_FUNCTION;
    m_ExePath = fs_path_current();
    m_UOPath = fs_path_current();
}

CApplication::~CApplication()
{
    DEBUG_TRACE_FUNCTION;
}

int CApplication::Run()
{
    bool quit = false;
    while (!quit)
    {
        // 1] Handle all SDL events. Mouse, keyboard, windows etc..
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
        {
            quit = Wisp::g_WispWindow->OnWindowProc(event);
            if (quit)
            {
                break;
            }
        }

        // 2] Run main loop, packets, rendering etc..
        OnMainLoop();

        // 3] Calculate proper delay based on settings and frame time.
        int32_t iDynamicFPS = SDL_GetTicks() - g_Ticks;
        int32_t iDelay = g_GameWindow.GetRenderDelay();

        SDL_Delay(std::max(iDelay - iDynamicFPS, CPU_USAGE_DELAY));
    }
    return EXIT_SUCCESS;
}

os_path CApplication::ExeFilePath(const char *str, ...) const
{
    DEBUG_TRACE_FUNCTION;
    va_list arg;
    va_start(arg, str);

    char out[MAX_PATH] = { 0 };
    vsprintf_s(out, str, arg);
    va_end(arg);

    os_path res = m_ExePath.c_str();
    return res + PATH_SEP + ToPath(out);
}

os_path CApplication::UOFilesPath(const string &str, ...) const
{
    return UOFilesPath(str.c_str());
}

os_path CApplication::UOFilesPath(const char *str, ...) const
{
    DEBUG_TRACE_FUNCTION;
    va_list arg;
    va_start(arg, str);

    char out[MAX_PATH] = { 0 };
    vsprintf_s(out, str, arg);
    va_end(arg);

    os_path res = m_UOPath.c_str();
    auto tmp = res + PATH_SEP + ToPath(out);
    return fs_insensitive(tmp);
}

}; // namespace Wisp
