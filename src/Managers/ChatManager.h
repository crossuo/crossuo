// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#ifndef MANAGERS_CHATMANAGER_H
#define MANAGERS_CHATMANAGER_H

#include <vector>
#include <common/str.h>

enum CHAT_STATUS
{
    CS_DISABLED = 0,
    CS_ENABLED = 1,
    CS_ENABLED_USER_REQUEST = 2
};

class CChatChannel
{
public:
    astr_t Name = {};
    bool HasPassword = false;

    CChatChannel() = default;
    CChatChannel(const astr_t &name, bool hasPassword)
        : Name(name)
        , HasPassword(hasPassword)
    {
    }
};

class CChatManager
{
public:
    std::vector<CChatChannel> Channels;
    CHAT_STATUS ChatStatus = CS_DISABLED;
    astr_t CurrentChannelName = {};

    CChatManager() = default;
    ~CChatManager() = default;

    void AddChannel(const astr_t &name, bool hasPassword);
    void RemoveChannel(const astr_t &name);
    void Clear();
    static const astr_t &GetMessage(int index);
};

extern CChatManager g_ChatManager;

#endif // MANAGERS_CHATMANAGER_H
