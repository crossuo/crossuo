// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Temporary shim for the model round: the Qt app called
// g_PacketManager.GetClientVersion() (its own negotiated version gate, default
// CV_OLD). The packet-manager round replaces this with the real accessor;
// until then the model behaves as it does before the version handshake.

#ifndef XUOASSIST_CLIENTVERSION_H
#define XUOASSIST_CLIENTVERSION_H

#include <xuocore/enumlist.h>

namespace xuoassist {

inline CLIENT_VERSION GetClientVersion()
{
    return CV_OLD;
}

} // namespace xuoassist

#endif // XUOASSIST_CLIENTVERSION_H
