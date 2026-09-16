// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (assist/packets.h). The builders are
// protocol-critical and byte-identical to the originals; do not "improve"
// them. Changes vs the original app (plan §4.1): the only framework bits were the `emit`
// marshaling calls in CPacket::SendClient/SendServer — in-process they go
// straight to the client (SendClient injects into the client's receive path,
// SendServer goes to g_ConnectionManager.Send).
// Lists are std::vector, ../managers/gumpmanager.h -> the ported CGump.
//
// The pure wire-send builders live in the client's src/Network/Packets.{h,cpp}
// (raw overloads on the client's own classes); what remains here are the
// client-bound injections (0x95 dye prompt, 0xD1 logout, 0xBF paperdoll close).

#pragma once

#include "global.h"
#include <xuocore/mappedfile.h> // ::CDataWriter (the client packet writer base)
#include "../model/commonitems/gump.h"

namespace xuoassist {

#define DEFINE_PACKET(name, ...)                                                                \
    class CPacket##name : public CPacket                                                           \
    {                                                                                              \
    public:                                                                                        \
        CPacket##name(__VA_ARGS__);                                                                \
    }

class CPacket : public CDataWriter
{
public:
    CPacket(size_t size, bool autoResize = false);

    void SendClient();
    void SendServer();
};

DEFINE_PACKET(DyeData, uint32_t serial);
DEFINE_PACKET(LogOut);
DEFINE_PACKET(ClosePaperdoll, uint32_t serial);

} // namespace xuoassist

