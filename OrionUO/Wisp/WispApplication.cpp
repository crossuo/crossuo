// MIT License

#include "FileSystem.h"
#include "WispThread.h"
#include <SDL_timer.h>
namespace Wisp
{
CApplication::CApplication()
{
}

void CApplication::Init()
{
    LOG("INITIATING CAPPLICATION\n");
    g_MainThread = CThread::GetCurrentThreadId();
    DEBUG_TRACE_FUNCTION;
    m_ExePath = fs_path_current();
    m_UOPath = fs_path_current();
    g_MainScreen.LoadCustomPath();
}

CApplication::~CApplication()
{
    DEBUG_TRACE_FUNCTION;
}

#if USE_WISP
int CApplication::Run(HINSTANCE hinstance)
{
    // DEBUG_TRACE_FUNCTION;
    timeBeginPeriod(1);
    Hinstance = hinstance;
    MSG msg = { 0 };
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            SDL_Delay(1);
        }
        OnMainLoop();
    }
    timeEndPeriod(1);
    return (int)msg.wParam;
#else
int CApplication::Run()
{
    bool quit = false;
    while (!quit)
    {
#if USE_TIMERTHREAD
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
        {
            quit = Wisp::g_WispWindow->OnWindowProc(event);
            if (quit)
            {
                break;
            }

            OnMainLoop();
        }
#else
        // 1] Handle all SDL events. Mouse, keyboard, windows etc..
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
        {
            quit = Wisp::g_WispWindow->OnWindowProc(event);
            if (quit)
                break;
        }

        // 2] Run main loop, packets, rendering etc..
        OnMainLoop();

        // 3] Calculate proper delay based on settings and frame time.
        int32_t iDynamicFPS = SDL_GetTicks() - g_Ticks;
        int32_t iDelay = g_OrionWindow.GetRenderDelay();

        SDL_Delay(std::max(iDelay - iDynamicFPS, CPU_USAGE_DELAY));
#endif // USE_TIMERTHREAD
    }
    return EXIT_SUCCESS;
#endif
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
