// MIT License

#include <algorithm>
#include <SDL_timer.h>
#include "WispThread.h"
#include "../GameWindow.h"
#include "../Globals.h" // g_Ticks, CPU_USAGE_DELAY

namespace Wisp
{
CApplication::CApplication()
{
}

void CApplication::Init()
{
    Info(Client, "initializing application");
    g_MainThread = CThread::GetCurrentThreadId();

    m_ExePath = fs_path_current();
    m_UOPath = fs_path_current();
}

CApplication::~CApplication()
{
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

fs_path CApplication::ExeFilePath(const char *str, ...) const
{
    va_list arg;
    va_start(arg, str);

    char out[FS_MAX_PATH] = { 0 };
    vsprintf_s(out, str, arg);
    va_end(arg);

    fs_path res = fs_path_join(m_ExePath, out);
    return fs_insensitive(res);
}

fs_path CApplication::UOFilesPath(const std::string &str, ...) const
{
    return UOFilesPath(str.c_str());
}

fs_path CApplication::UOFilesPath(const char *str, ...) const
{
    va_list arg;
    va_start(arg, str);

    char out[FS_MAX_PATH] = { 0 };
    vsprintf_s(out, str, arg);
    va_end(arg);

    fs_path res = fs_path_join(m_UOPath, out);
    return fs_insensitive(res);
}

}; // namespace Wisp
