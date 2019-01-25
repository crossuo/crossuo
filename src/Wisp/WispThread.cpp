// MIT License

#include <thread>
#include <SDL_timer.h>
#include <SDL_thread.h>
#define THREAD_USE_CLOCK 0

#define THREADCALL SDLCALL
typedef int thread_int;

namespace Wisp
{
thread_int THREADCALL CThreadLoop(void *arg)
{
    DEBUG_TRACE_FUNCTION;
    CThread *parent = (CThread *)arg;

    while (parent->IsActive())
    {
        while (parent->Paused())
        {
            SDL_Delay(1);
        }

#if THREAD_USE_CLOCK == 1
        parent->OnExecute(clock());
#else
        parent->OnExecute(SDL_GetTicks());
#endif

        if (!parent->Cycled())
        {
            break;
        }

        int delay = parent->Delay();

        if (delay > 0)
        {
            SDL_Delay(delay);
        }
    }

    parent->OnDestroy();
    delete parent;

    return 0;
};

thread_int THREADCALL CThreadLoopSynchronizedDelay(void *arg)
{
    DEBUG_TRACE_FUNCTION;
    CThread *parent = (CThread *)arg;

    while (parent->IsActive())
    {
        while (parent->Paused())
        {
            SDL_Delay(1);
        }

#if THREAD_USE_CLOCK == 1
        uint32_t nowTime = clock();
#else
        uint32_t nowTime = SDL_GetTicks();
#endif

        parent->OnExecute(nowTime);
        if (!parent->Cycled())
        {
            break;
        }

#if THREAD_USE_CLOCK == 1
        int delay = (int)((nowTime + parent->Delay()) - clock());
#else
        int delay = (int)((nowTime + parent->Delay()) - SDL_GetTicks());
#endif

        SDL_Delay(delay > 0 ? delay : 1);
    }

    parent->OnDestroy();
    delete parent;

    return 0;
};

CThread::CThread()
{
    DEBUG_TRACE_FUNCTION;
    CREATE_MUTEX(m_Mutex);
}

CThread::~CThread()
{
}

void CThread::OnDestroy()
{
    DEBUG_TRACE_FUNCTION;
    RELEASE_MUTEX(m_Mutex);
    if (m_Handle != nullptr)
    {
        SDL_DetachThread(m_Handle);
    }
    m_Handle = nullptr;
    m_ID = 0;
}

void CThread::Run(bool cycled, int delay, bool synchronizedDelay)
{
    DEBUG_TRACE_FUNCTION;
    if (!m_Active && m_Handle == 0)
    {
        m_Cycled = cycled;
        m_Delay = delay;
        m_Active = true;
        if (synchronizedDelay)
        {
            m_Handle = SDL_CreateThread(
                CThreadLoopSynchronizedDelay, "CThreadLoopSynchronizedDelay", (void *)this);
        }
        else
        {
            m_Handle = SDL_CreateThread(CThreadLoop, "CThreadLoop", (void *)this);
        }
    }
}

bool CThread::IsActive()
{
    DEBUG_TRACE_FUNCTION;
    LOCK(m_Mutex);
    bool result = m_Active;
    UNLOCK(m_Mutex);
    return result;
}

void CThread::Stop()
{
    DEBUG_TRACE_FUNCTION;
    LOCK(m_Mutex);
    m_Active = false;
    UNLOCK(m_Mutex);
}

bool CThread::Cycled()
{
    DEBUG_TRACE_FUNCTION;
    LOCK(m_Mutex);
    bool result = m_Cycled;
    UNLOCK(m_Mutex);
    return result;
}

void CThread::Pause()
{
    DEBUG_TRACE_FUNCTION;
    LOCK(m_Mutex);
    m_Paused = true;
    UNLOCK(m_Mutex);
}

void CThread::Resume()
{
    DEBUG_TRACE_FUNCTION;
    LOCK(m_Mutex);
    m_Paused = false;
    UNLOCK(m_Mutex);
}

bool CThread::Paused()
{
    DEBUG_TRACE_FUNCTION;
    LOCK(m_Mutex);
    bool result = m_Paused;
    UNLOCK(m_Mutex);
    return result;
}

int CThread::Delay()
{
    DEBUG_TRACE_FUNCTION;
    LOCK(m_Mutex);
    int result = m_Delay;
    UNLOCK(m_Mutex);
    return result;
}

void CThread::ChangeDelay(int newDelay)
{
    DEBUG_TRACE_FUNCTION;
    LOCK(m_Mutex);
    m_Delay = newDelay;
    UNLOCK(m_Mutex);
}

SDL_threadID CThread::GetCurrentThreadId()
{
    return SDL_GetThreadID(nullptr);
}

}; // namespace Wisp
