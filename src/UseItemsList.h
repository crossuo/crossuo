// MIT License
// Copyright (C) September 2017 Hotride

#pragma once

class CUseItemActions
{
    uint32_t Timer = 0;

private:
    std::deque<uint32_t> m_List;

public:
    CUseItemActions() {}
    ~CUseItemActions() {}

    void Add(int serial);

    void Clear() { m_List.clear(); }

    void Process();
};

extern CUseItemActions g_UseItemActions;
