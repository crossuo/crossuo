// MIT License
// Copyright (C) December 2017 Hotride

#if USE_PING

#include <SDL_events.h>
#include <SDL_timer.h>
#include <atomic>

#include "PingThread.h"

static std::atomic<int32_t> s_pingCnt;

CPingThread::CPingThread(int serverID, const string &serverIP, int requestsCount)
    : ServerID(serverID)
    , ServerIP(serverIP)
    , RequestsCount(requestsCount)
{
    DEBUG_TRACE_FUNCTION;
    //assert(s_pingCnt == 0 && "Multiple ping threads running at the same time");
    s_pingCnt++;
    LOG("CPingThread => %s\n", serverIP.c_str());
}

CPingThread::~CPingThread()
{
    DEBUG_TRACE_FUNCTION;
    //assert(s_pingCnt == 1 && "Multiple ping threads running at the same time");
    s_pingCnt--;
}

int CPingThread::CalculatePing()
{
    auto handle = icmp_open();
    if (handle == nullptr)
    {
        return -4;
    }

    uint32_t start = SDL_GetTicks();
    int result = icmp_query(handle, ServerIP.c_str(), &start);
    if (result == 0)
    {
        result = (SDL_GetTicks() - start);
    }

    icmp_close(handle);
    return result;
}

void CPingThread::OnExecute(uint32_t nowTime)
{
    DEBUG_TRACE_FUNCTION;

    if (ServerIP.empty() || RequestsCount < 1)
    {
        return;
    }

    auto *info = new PING_INFO_DATA;
    info->ServerID = ServerID;
    for (int i = 0; i < RequestsCount; i++)
    {
        const int ping = CalculatePing();
        if (ping < 0)
        {
            if (ping == -1)
            {
                info->Lost++;
            }
            continue;
        }

        info->Min = std::min(info->Min, ping);
        info->Max = std::max(info->Max, ping);
        info->Average += (info->Max - info->Min);
    }

    info->Average = info->Min + (info->Average / RequestsCount);
    PUSH_EVENT(MessageID, info, nullptr);
}

#endif // USE_PING
