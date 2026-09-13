// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Resolved at the packet-manager round: the Qt app read the host's validated
// client version once at plugin install (mainscript.cpp:397,
// g_ClientInterface->ClientVersion). In-process, the client's own negotiated
// version (Config::ClientVersion, validated from the exe at startup and sent
// in the 0xBD handshake) is the authoritative source.

#pragma once

#include <xuocore/enumlist.h>
#include "Config.h" // ::g_Config

namespace xuoassist {

inline CLIENT_VERSION GetClientVersion()
{
    return (CLIENT_VERSION)g_Config.ClientVersion;
}

} // namespace xuoassist

