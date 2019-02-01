// MIT License
// Copyright (C) December 2017 Hotride

#pragma once

#if USE_PING
struct PING_INFO_DATA
{
    uint32_t ServerID = 0;
    int Min = 9999;
    int Max = 0;
    int Average = 0;
    int Lost = 0;
};

class CPingThread : public Wisp::CThread
{
    uint32_t ServerID = 0xFFFFFFFF;
    string ServerIP = "";
    int RequestsCount = 10;

private:
    int CalculatePing();

public:
    CPingThread(int serverID, const string &serverIP, int requestsCount);
    virtual ~CPingThread();

    virtual void OnExecute(uint32_t nowTime) override;
    static const uint32_t MessageID = USER_MESSAGE_ID + 401;
};
#endif // USE_PING
