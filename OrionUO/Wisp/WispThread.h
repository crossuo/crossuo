// MIT License

#ifndef WISP_THREAD_H
#define WISP_THREAD_H

#include "../Platform.h"
#include <SDL_thread.h>

namespace Wisp
{
class CThread
{
private:
#if USE_WISP
    HANDLE m_Handle = 0;
#else
    SDL_Thread *m_Handle = nullptr;
#endif
    ProtectedSection m_Mutex;
    uint32_t m_ID = 0;
    bool m_Paused = false;
    bool m_Cycled = false;
    int m_Delay = 1;
    bool m_Active = false;

public:
    CThread();
    virtual ~CThread();

    void Run(bool cycled = false, int delay = 1, bool synchronizedDelay = false);

    bool IsActive();
    void Stop();
    bool Cycled();

    void Pause();
    void Resume();
    bool Paused();

    int Delay();
    void ChangeDelay(int newDelay);

    virtual void OnExecute(uint32_t nowTime) {}
    virtual void OnDestroy();

    static SDL_threadID GetCurrentThreadId();
};

}; // namespace Wisp

#endif // WISP_THREAD_H
