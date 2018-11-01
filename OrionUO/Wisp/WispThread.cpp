// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "stdafx.h"

#include <thread>
#include <SDL_timer.h>
#include <SDL_thread.h>
#define THREAD_USE_CLOCK 0

#if USE_WISP
#define THREADCALL __stdcall
typedef unsigned thread_int;
#else
#define THREADCALL SDLCALL
typedef int thread_int;
#endif

namespace Wisp
{

thread_int THREADCALL CThreadLoop(void *arg)
{
    DEBUG_TRACE_FUNCTION;
    CThread *parent = (CThread *)arg;

    while (parent->IsActive())
    {
        while (parent->Paused())
            SDL_Delay(1);

#if THREAD_USE_CLOCK == 1
        parent->OnExecute(clock());
#else
        parent->OnExecute(SDL_GetTicks());
#endif

        if (!parent->Cycled())
            break;
        else
        {
            int delay = parent->Delay();

            if (delay > 0)
                SDL_Delay(delay);
        }
    }

    parent->OnDestroy();
    delete parent;

#if USE_WISP
    _endthreadex(0);
#endif

    return 0;
};

thread_int THREADCALL CThreadLoopSynchronizedDelay(void *arg)
{
    DEBUG_TRACE_FUNCTION;
    CThread *parent = (CThread *)arg;

    while (parent->IsActive())
    {
        while (parent->Paused())
            SDL_Delay(1);

#if THREAD_USE_CLOCK == 1
        uint32_t nowTime = clock();
#else
        uint32_t nowTime = SDL_GetTicks();
#endif

        parent->OnExecute(nowTime);

        if (!parent->Cycled())
            break;
        else
        {
#if THREAD_USE_CLOCK == 1
            int delay = (int)((nowTime + parent->Delay()) - clock());
#else
            int delay = (int)((nowTime + parent->Delay()) - SDL_GetTicks());
#endif

            SDL_Delay(delay > 0 ? delay : 1);
        }
    }

    parent->OnDestroy();
    delete parent;

#if USE_WISP
    _endthreadex(0);
#endif

    return 0;
};

CThread::CThread()
{
    DEBUG_TRACE_FUNCTION;
    //DebugMsg("CThread\n");
#if USE_WISP
    InitializeCriticalSection(&m_CriticalSection);
#else
    m_Mutex = SDL_CreateMutex();
#endif
}

CThread::~CThread()
{
    DEBUG_TRACE_FUNCTION;
    //DebugMsg("~CThread\n");
#if USE_WISP
    DeleteCriticalSection(&m_CriticalSection);

    if (m_Handle != 0)
        ::CloseHandle(m_Handle);
    m_Handle = 0;
#else
    if (!m_Mutex)
        SDL_DestroyMutex(m_Mutex);
    m_Mutex = nullptr;

    int ret = 0;
    if (m_Handle)
        SDL_WaitThread(m_Handle, &ret);
    m_Handle = nullptr;
#endif
    ID = 0;
}

void CThread::Run(bool cycled, int delay, bool synchronizedDelay)
{
    DEBUG_TRACE_FUNCTION;
    if (!m_Active && m_Handle == 0)
    {
        m_Cycled = cycled;
        m_Delay = delay;
        m_Active = true;

#if USE_WISP
        if (synchronizedDelay)
            m_Handle = (HANDLE)_beginthreadex(nullptr, 0, CThreadLoopSynchronizedDelay, this, 0, &ID);
        else
            m_Handle = (HANDLE)_beginthreadex(nullptr, 0, CThreadLoop, this, 0, &ID);
#else
        if (synchronizedDelay)
            m_Handle = SDL_CreateThread(
                CThreadLoopSynchronizedDelay, "CThreadLoopSynchronizedDelay", (void *)this);
        else
            m_Handle = SDL_CreateThread(CThreadLoop, "CThreadLoop", (void *)this);
#endif
    }
}

bool CThread::IsActive()
{
    DEBUG_TRACE_FUNCTION;
#if USE_WISP
    EnterCriticalSection(&m_CriticalSection);
    bool result = m_Active;
    LeaveCriticalSection(&m_CriticalSection);
#else
    SDL_LockMutex(m_Mutex);
    bool result = m_Active;
    SDL_UnlockMutex(m_Mutex);
#endif
    return result;
}

void CThread::Stop()
{
    DEBUG_TRACE_FUNCTION;
#if USE_WISP
    EnterCriticalSection(&m_CriticalSection);
    m_Active = false;
    LeaveCriticalSection(&m_CriticalSection);
#else
    SDL_LockMutex(m_Mutex);
    m_Active = false;
    SDL_UnlockMutex(m_Mutex);
#endif
}

bool CThread::Cycled()
{
    DEBUG_TRACE_FUNCTION;
#if USE_WISP
    EnterCriticalSection(&m_CriticalSection);
    bool result = m_Cycled;
    LeaveCriticalSection(&m_CriticalSection);
#else
    SDL_LockMutex(m_Mutex);
    bool result = m_Cycled;
    SDL_UnlockMutex(m_Mutex);
#endif

    return result;
}

void CThread::Pause()
{
    DEBUG_TRACE_FUNCTION;
#if USE_WISP
    EnterCriticalSection(&m_CriticalSection);
    m_Paused = true;
    LeaveCriticalSection(&m_CriticalSection);
#else
    SDL_LockMutex(m_Mutex);
    m_Paused = true;
    SDL_UnlockMutex(m_Mutex);
#endif
}

void CThread::Resume()
{
    DEBUG_TRACE_FUNCTION;
#if USE_WISP
    EnterCriticalSection(&m_CriticalSection);
    m_Paused = false;
    LeaveCriticalSection(&m_CriticalSection);
#else
    SDL_LockMutex(m_Mutex);
    m_Paused = false;
    SDL_UnlockMutex(m_Mutex);
#endif
}

bool CThread::Paused()
{
    DEBUG_TRACE_FUNCTION;
#if USE_WISP
    EnterCriticalSection(&m_CriticalSection);
    bool result = m_Paused;
    LeaveCriticalSection(&m_CriticalSection);
#else
    SDL_LockMutex(m_Mutex);
    bool result = m_Paused;
    SDL_UnlockMutex(m_Mutex);
#endif

    return result;
}

int CThread::Delay()
{
    DEBUG_TRACE_FUNCTION;
#if USE_WISP
    EnterCriticalSection(&m_CriticalSection);
    int result = m_Delay;
    LeaveCriticalSection(&m_CriticalSection);
#else
    SDL_LockMutex(m_Mutex);
    int result = m_Delay;
    SDL_UnlockMutex(m_Mutex);
#endif

    return result;
}

void CThread::ChangeDelay(int newDelay)
{
    DEBUG_TRACE_FUNCTION;
#if USE_WISP
    EnterCriticalSection(&m_CriticalSection);
    m_Delay = newDelay;
    LeaveCriticalSection(&m_CriticalSection);
#else
    SDL_LockMutex(m_Mutex);
    m_Delay = newDelay;
    SDL_UnlockMutex(m_Mutex);
#endif
}

SDL_threadID CThread::GetCurrentThreadId()
{
    //return GetCurrentThreadId();
    //return std::this_thread::get_id();
    return SDL_GetThreadID(nullptr);
}

}; // namespace Wisp

