// MIT License

#include <SDL_events.h>

namespace Wisp
{
static uint32_t UserEventID = 0;

CThreadedTimer::CThreadedTimer(uint32_t id, WindowHandle handle, bool waitForProcessMessage)
    : Wisp::CThread()
    , TimerID(id)
    , m_Handle(handle)
    , WaitForProcessMessage(waitForProcessMessage)
{
#if !USE_WISP
    if (!UserEventID)
    {
        UserEventID = SDL_RegisterEvents(1);
    }
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
            SendMessage(m_Handle, MessageID, nowTime, (LPARAM)this);
        else
            PostMessage(m_Handle, MessageID, nowTime, (LPARAM)this);
#else
        SDL_Event event;
        SDL_zero(event);
        event.type = UserEventID;
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
    {
        Wisp::g_WispWindow->RemoveThreadedTimer(TimerID);
    }
}

};