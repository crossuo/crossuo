// MIT License

#include <SDL_events.h>

namespace Wisp
{
CThreadedTimer::CThreadedTimer(uint32_t id, HWND windowHandle, bool waitForProcessMessage)
    : Wisp::CThread()
    , TimerID(id)
    , WindowHandle(windowHandle)
    , WaitForProcessMessage(waitForProcessMessage)
{
#if !USE_WISP
    EventID = SDL_RegisterEvents(1);
#endif
}

CThreadedTimer::~CThreadedTimer()
{
}

void CThreadedTimer::OnExecute(uint32_t nowTime)
{
    DEBUG_TRACE_FUNCTION;
    if (IsActive())
    {
#if USE_WISP
        if (WaitForProcessMessage)
            SendMessage(WindowHandle, MessageID, nowTime, (LPARAM)this);
        else
            PostMessage(WindowHandle, MessageID, nowTime, (LPARAM)this);
#else
        SDL_Event event;
        SDL_zero(event);
        event.type = EventID;
        event.user.code = MessageID;
        event.user.data1 = (void *)nowTime;
        event.user.data2 = this;
        SDL_PushEvent(&event);
#endif
    }
}

void CThreadedTimer::OnDestroy()
{
    DEBUG_TRACE_FUNCTION;
    if (Wisp::g_WispWindow != nullptr)
        Wisp::g_WispWindow->RemoveThreadedTimer(TimerID);
}

}; // namespace Wisp
