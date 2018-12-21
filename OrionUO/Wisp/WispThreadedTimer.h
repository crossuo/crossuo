// MIT License

#if USE_TIMERTHREAD

#pragma once

namespace Wisp
{
class CThreadedTimer : public Wisp::CThread
{
public:
    static const uint32_t MessageID = USER_MESSAGE_ID + 400;
    WindowHandle m_Handle = 0;
    uint32_t TimerID = 0;
    bool WaitForProcessMessage = false;

    CThreadedTimer(uint32_t id, WindowHandle handle, bool waitForProcessMessage = false);
    virtual ~CThreadedTimer();

    virtual void OnExecute(uint32_t nowTime);
    virtual void OnDestroy();
};

}; // namespace Wisp

#endif // USE_TIMERTHREAD
