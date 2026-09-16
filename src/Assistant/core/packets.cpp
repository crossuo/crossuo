// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (assist/packets.cpp). Builder bodies are byte
// identical to the originals (protocol-critical; plan "must-follow" rule).
// Only the transport changed: the original app's `emit g_Assistant.signal_*`
// cross-thread marshaling becomes direct client calls (plan §4.1):
//   SendClient -> client receive injection (queued to the client's packet loop)
//   SendServer -> g_ConnectionManager.Send
// The pure wire-send builders moved to the client's src/Network/Packets.{h,cpp}
// (raw overloads on the client's own classes); what remains here are the
// client-bound injections (0x95 dye prompt, 0xD1 logout, 0xBF paperdoll close).

// SDL types must come first: xuocore/plugininterface.h maps its event structs
// onto SDL's when SDL headers are visible (USING_SDL_STRUCTS), which is what
// makes the client's PacketManager.h (Wisp/Platform) coexist with it here.
#include <SDL_wrapper.h>
#include "packets.h"
#include "Managers/PacketManager.h"      // ::g_PacketManager (client, inject)
#include "Managers/ConnectionManager.h" // ::g_ConnectionManager (client, send)

namespace xuoassist {

CPacket::CPacket(size_t size, bool autoResize)
    : CDataWriter(size, autoResize)
{
}

void CPacket::SendClient()
{
    if (!m_Data.empty())
    {
        // client-bound injection: queued for the client's packet loop
        ::g_PacketManager.SaveAssistantReceivePacket(&m_Data[0], (int)m_Data.size());
    }
}

void CPacket::SendServer()
{
    if (!m_Data.empty())
    {
        // direct wire send; the assistant's own send tap does not see it
        ::g_ConnectionManager.Send(&m_Data[0], (int)m_Data.size());
    }
}

CPacketDyeData::CPacketDyeData(uint32_t serial)
    : CPacket(9)
{
    WriteUInt8(0x95);
    WriteUInt32BE(serial);
    WriteUInt16BE(0);
    WriteUInt16BE(0x0FAB);
}

CPacketLogOut::CPacketLogOut()
    : CPacket(2)
{
    WriteUInt8(0xD1);
    WriteUInt8(0x00);
}

CPacketClosePaperdoll::CPacketClosePaperdoll(uint32_t serial)
    : CPacket(13)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(13);
    WriteUInt16BE(0x0016);
    WriteUInt32BE(1);
    WriteUInt32BE(serial);
}

} // namespace xuoassist
