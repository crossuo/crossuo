// MIT License

#include "FileSystem.h"
#include "WispThread.h"
#include <SDL_timer.h>
namespace Wisp
{
CApplication *g_WispApplication = nullptr;

CApplication::CApplication()
{
    LOG("INITIATING CAPPLICATION\n");
    g_MainThread = CThread::GetCurrentThreadId();
    DEBUG_TRACE_FUNCTION;
    g_WispApplication = this;
    m_ExePath = fs_path_current();
    m_UOPath = fs_path_current();
    g_MainScreen.LoadCustomPath();
}

CApplication::~CApplication()
{
    DEBUG_TRACE_FUNCTION;
    g_WispApplication = nullptr;
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
    return res + PATH_SEP + ToPath(out);
}

}; // namespace Wisp
