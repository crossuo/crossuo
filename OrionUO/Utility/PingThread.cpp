// MIT License
// Copyright (C) December 2017 Hotride

#include <SDL_events.h>
#include <SDL_timer.h>

CPingThread::CPingThread(int serverID, const string &serverIP, int requestsCount)
    : Wisp::CThread()
    , ServerID(serverID)
    , ServerIP(serverIP)
    , RequestsCount(requestsCount)
{
    DEBUG_TRACE_FUNCTION;
    LOG("CPingThread => %s\n", serverIP.c_str());
}

CPingThread::~CPingThread()
{
    DEBUG_TRACE_FUNCTION;
}

int CPingThread::CalculatePing()
{
    auto handle = icmp_open();
    if (!handle)
        return -4;

    uint32_t timems = SDL_GetTicks();
    int result = icmp_query(handle, ServerIP.c_str(), &timems);
    if (!result)
        result = (SDL_GetTicks() - timems);

    icmp_close(handle);
    return result;
}

void CPingThread::OnExecute(uint32_t nowTime)
{
    DEBUG_TRACE_FUNCTION;

    if (ServerIP.empty() || RequestsCount < 1)
        return;

    PING_INFO_DATA info = { ServerID, 9999, 0, 0, 0 };
    for (int i = 0; i < RequestsCount; i++)
    {
        const int ping = CalculatePing();
        if (ping < 0)
        {
            if (ping == -1)
                info.Lost++;

            continue;
        }

        info.Min = min(info.Min, ping);
        info.Max = max(info.Max, ping);
        info.Average += (info.Max - info.Min);
    }

    info.Average = info.Min + (info.Average / RequestsCount);
    PUSH_EVENT(MessageID, &info, nullptr);
}
