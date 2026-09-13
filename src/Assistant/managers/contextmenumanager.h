// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/contextmenumanager.h), trimmed.

#pragma once

#include "core/global.h"
#include <xuocore/mappedfile.h> // ::CDataReader (the client data reader)

namespace xuoassist {

class CContextMenuManager
{
    SETGET(uint, Serial, 0)
    SETGET(bool, HookEnabled, false)
    SETGET(uint, HookID, 0)
    SETGET(uint, HookIndex, 0)

public:
    CContextMenuManager();

    void SetHook(const uint &id, const uint &index);

    void RemoveHook();

    bool OnPacketReceived(CDataReader &reader);

    bool OnPacketSend(CDataReader &reader);
};

extern CContextMenuManager g_ContextMenuManager;

} // namespace xuoassist

