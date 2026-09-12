// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "assistantevents.h"
#include "packets.h"
#include <common/str.h> // wstr_from

namespace xuoassist {

namespace {

// The do-nothing sink every handler sees before a UI installs a subclass.
class CDefaultAssistantEvents : public CAssistantEvents
{
};

CDefaultAssistantEvents s_DefaultAssistantEvents;

} // namespace

CAssistantEvents::~CAssistantEvents()
{
}

CAssistantEvents *g_AssistantEvents = &s_DefaultAssistantEvents;

// Original: CAssistant::ClientPrint -> CPacketToClientSystemPrint(text).SendClient()
void CAssistantEvents::OnClientPrint(const string &text, ushort color)
{
    CPacketToClientSystemPrint(wstr_from(text), color).SendClient();
}

} // namespace xuoassist
