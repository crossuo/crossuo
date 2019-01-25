// MIT License

#pragma once

class AutoResetEvent
{
public:
    explicit AutoResetEvent(bool initial = false);

    void Set();
    void Reset();

    bool WaitOne();

private:
    AutoResetEvent(const AutoResetEvent &);
    AutoResetEvent &operator=(const AutoResetEvent &); // non-copyable
    bool flag_;
    std::mutex protect_;
    std::condition_variable signal_;
};
