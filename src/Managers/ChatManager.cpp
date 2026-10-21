// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "ChatManager.h"
#include <cstddef>

CChatManager g_ChatManager;

static const astr_t g_ChatMessages[] = {
    "You are already ignoring the maximum number of players.",
    "You are already ignoring %1.",
    "You are now ignoring %1.",
    "You are no longer ignoring %1.",
    "You are not ignoring %1.",
    "You are no longer ignoring anyone.",
    "That is not a valid conference name.",
    "There is already a conference with that name.",
    "You must have operator status to do this.",
    "The conference %1 has been renamed to %2.",
    "You must be in a conference to do this.",
    "There is no player named '%1'.",
    "There is no conference named '%1'.",
    "That is not the correct password.",
    "%1 has chosen to ignore you. Neither of you will see the other's messages.",
    "%1 is not giving you speaking privileges in this conference.",
    "You can now receive private messages from everyone.",
    "You will no longer receive private messages from everyone.",
    "You are now showing your character name to everyone.",
    "You are now hiding your character name from everyone.",
    "%1 is remaining anonymous.",
    "%1 has chosen to not receive private messages.",
    "%1 is known in the lands of Britannia as %2.",
    "%1 has been kicked out of the conference.",
    "A conference moderator has kicked you out of the conference.",
    "You are already in the conference '%1'.",
    "%1 is no longer a conference moderator.",
    "%1 is now a conference moderator.",
    "%1 has removed you from the conference moderators.",
    "%1 has made you a conference moderator.",
    "%1 no longer has speaking privileges in this conference.",
    "%1 now has speaking privileges in this conference.",
    "%1 has removed your speaking privileges in this conference.",
    "%1 has granted you speaking privileges in this conference.",
    "Everyone will now have speaking privileges in this conference.",
    "Only moderators will now have speaking privileges in this conference.",
    "The password to the conference has been changed.",
    "The conference '%1' is full. You cannot join at this time.",
    "You are banning %1 from this conference.",
    "%1 has banned you from the conference.",
    "%1 has been banned from the conference."
};

void CChatManager::AddChannel(const astr_t &name, bool hasPassword)
{
    for (const CChatChannel &channel : Channels)
    {
        if (channel.Name == name)
        {
            return;
        }
    }

    Channels.push_back(CChatChannel(name, hasPassword));
}

void CChatManager::RemoveChannel(const astr_t &name)
{
    for (size_t i = 0; i < Channels.size(); i++)
    {
        if (Channels[i].Name == name)
        {
            Channels.erase(Channels.begin() + i);
            return;
        }
    }
}

void CChatManager::Clear()
{
    Channels.clear();
    CurrentChannelName = {};
}

const astr_t &CChatManager::GetConferenceMessage(int index)
{
    static const astr_t empty;
    if (index < 0 || index >= (int)(sizeof(g_ChatMessages) / sizeof(g_ChatMessages[0])))
    {
        return empty;
    }
    return g_ChatMessages[index];
}
