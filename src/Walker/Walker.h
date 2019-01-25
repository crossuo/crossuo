// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CStepInfo
{
public:
    uint8_t Direction = 0;
    uint8_t OldDirection = 0;
    uint8_t Sequence = 0;
    bool Accepted = false;
    bool Running = false;
    bool NoRotation = false;
    uint32_t Timer = 0;
    uint16_t X = 0;
    uint16_t Y = 0;
    uint8_t Z = 0;

    CStepInfo() {}
    ~CStepInfo() {}
};

class CWalker
{
public:
    uint32_t LastStepRequestTime = 0;
    int UnacceptedPacketsCount = 0;
    int StepsCount = 0;
    uint8_t WalkSequence = 0;
    uint8_t CurrentWalkSequence = 0;
    bool ResendPacketSended = false;
    bool WantChangeCoordinates = false;
    bool WalkingFailed = false;
    uint16_t CurrentPlayerZ = 0;
    uint16_t NewPlayerZ = 0;

    CWalker();
    ~CWalker() {}

    CStepInfo m_Step[MAX_STEPS_COUNT];

    void Reset();
    void DenyWalk(uint8_t sequence, int x, int y, char z);
    void ConfirmWalk(uint8_t sequence);
};

extern CWalker g_Walker;
