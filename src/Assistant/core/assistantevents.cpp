// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "assistantevents.h"
#include "Managers/PacketManager.h" // ::g_PacketManager (client, talk display core)
#include <xuocore/enumlist.h> // ST_NORMAL (the talk handler's system-line routing)
#include <common/str.h> // wstr_from
#include <config/assistantconfig.h>

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

void CAssistantEvents::OnClientPrint(const astr_t &text, ushort color)
{
    ::g_PacketManager.DisplayTalkTextW(
        0xFFFFFFFF, nullptr, "X:UO Assist", wstr_from(text), color, ST_NORMAL);
}

void CAssistantEvents::OnMapData(uint16_t x, uint16_t y)
{
    x *= 2;
    y *= 2;

    x += g_LastTMapX;
    y += g_LastTMapY;

    astr_t text = "";
    {
        char buf[64] = { 0 };
        snprintf(buf, sizeof(buf), "g_Map opened to: %i:%i", x, y);
        text = buf;
    }
    g_AssistantEvents->OnClientPrint(text);

    {
        char buf[64] = { 0 };
        snprintf(buf, sizeof(buf), "mapecho: x=%i y=%i", x, y);
        text = buf;
    }
    CJournalMessage message(0xFFFFFFFF, 0x0000, text);
    message.Flags = JMT_SYSTEM;

    g_AssistantEvents->OnJournalMessage(message);

    // was: if (g_AssistantConfig.Tracker()) g_Assistant.Track(true, x, y)
    // (the assistant map draws the point now - see CAssistant::Track)
    if (g_AssistantConfig.Tracker())
    {
        g_AssistantEvents->OnTrack(true, x, y);
    }
}

namespace {

// The 0xDF buff icon-index -> script-visible name table from the old
// recv-tap handler; entries the original left unnamed keep the hex
// placeholder.
const int nameTableCount = 126;
static const astr_t nameTable[nameTableCount] = { "dismount",
                                                   "disarm",
                                                   "0x0000",
                                                   "0x0000",
                                                   "night sight",
                                                   "death strike",
                                                   "evil omen",
                                                   "0x7556",
                                                   "regeneration",
                                                   "divine fury",
                                                   "enemy of one",
                                                   "hiding",
                                                   "meditation",
                                                   "blood oath caster",
                                                   "blood oath",
                                                   "corpse skin",
                                                   "mind rot",
                                                   "pain spike",
                                                   "strangle",
                                                   "gift of renewal",
                                                   "attune weapon",
                                                   "thunderstorm",
                                                   "essence of wind",
                                                   "ethereal voyage",
                                                   "gift of life",
                                                   "arcane empowerment",
                                                   "mortal strike",
                                                   "reactive armor",
                                                   "protection",
                                                   "arch protection",
                                                   "magic reflection",
                                                   "incognito",
                                                   "disguised",
                                                   "animal form",
                                                   "polymorph",
                                                   "invisibility",
                                                   "paralyze",
                                                   "poison",
                                                   "bleed",
                                                   "clumsy",
                                                   "feeblemind",
                                                   "weaken",
                                                   "curse",
                                                   "mass curse",
                                                   "agility",
                                                   "cunning",
                                                   "strength",
                                                   "bless",
                                                   "sleep",
                                                   "0x758B",
                                                   "spell plague",
                                                   "0x758D",
                                                   "0x0000",


                                                   "0x094B",
                                                   "0x094C",
                                                   "0x094D",
                                                   "0x094E",
                                                   "0x094F",
                                                   "0x0950",
                                                   "0x753E",
                                                   "0x5011",
                                                   "0x7590",
                                                   "0x7591",
                                                   "0x7592",
                                                   "0x7593",
                                                   "0x7594",
                                                   "0x7595",
                                                   "0x7596",
                                                   "0x7598",
                                                   "0x7599",
                                                   "0x759B",
                                                   "0x759C",
                                                   "0x759E",
                                                   "0x759F",
                                                   "0x75A0",
                                                   "0x75A1",
                                                   "0x75A3",
                                                   "0x75A4",
                                                   "0x75A5",
                                                   "0x75A6",
                                                   "0x75A7",
                                                   "0x75C0",
                                                   "0x75C1",
                                                   "0x75C2",
                                                   "0x75C3",
                                                   "0x75C4",
                                                   "0x75F2",
                                                   "0x75F3",
                                                   "0x75F4",
                                                   "0x75F5",
                                                   "0x75F6",
                                                   "0x75F7",
                                                   "0x75F8",
                                                   "counter attack",
                                                   "0x75FA",
                                                   "0x75FB",
                                                   "0x75FC",
                                                   "0x75FD",
                                                   "0x75FE",
                                                   "0x75FF",
                                                   "0x7600",
                                                   "0x7601",
                                                   "0x7602",
                                                   "0x7603",
                                                   "0x7604",
                                                   "0x7605",
                                                   "0x7606",
                                                   "0x7607",
                                                   "0x7608",
                                                   "0x7609",
                                                   "0x760A",
                                                   "0x760B",
                                                   "0x760C",
                                                   "0x760D",
                                                   "0x760E",
                                                   "0x760F",
                                                   "0x7610",
                                                   "0x7611",
                                                   "0x7612",
                                                   "0x7613",
                                                   "0x7614",
                                                   "0x7615",
                                                   "0x75C5",
                                                   "0x75F6",
                                                   "0x761B" };

} // namespace

const astr_t &AssistantBuffName(ushort iconIndex)
{
    static const astr_t unnamed;
    if (iconIndex >= nameTableCount)
    {
        return unnamed;
    }

    return nameTable[iconIndex];
}

} // namespace xuoassist
