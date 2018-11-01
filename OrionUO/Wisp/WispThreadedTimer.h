
#ifndef WISPTHREADEDTIMER_H
#define WISPTHREADEDTIMER_H

namespace Wisp
{
class CThreadedTimer : public Wisp::CThread
{
public:
    uint32_t TimerID = 0;
    HWND WindowHandle = 0;
    bool WaitForProcessMessage = false;

    CThreadedTimer(uint32_t id, HWND windowHandle, bool wairForProcessMessage = false);
    virtual ~CThreadedTimer();

    static const uint32_t MessageID = WM_USER + 400;
    uint32_t EventID = 0;

    virtual void OnExecute(uint32_t nowTime);
    virtual void OnDestroy();
};

}; // namespace Wisp

#endif
