// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/contextmenumanager.h), trimmed.

#pragma once

#include "core/global.h"
#include <xuocore/mappedfile.h> // ::CDataReader (the client data reader)

namespace xuoassist {

class CContextMenuManager
{
public:
    uint32_t Serial = 0;
    bool HookEnabled = false;
    uint32_t HookID = 0;
    uint32_t HookIndex = 0;

    CContextMenuManager();

    CContextMenuManager(const CContextMenuManager &) = delete;
    CContextMenuManager &operator=(const CContextMenuManager &) = delete;

    void SetHook(const uint32_t &id, const uint32_t &index);

    void RemoveHook();

    bool OnPacketReceived(CDataReader &reader);

    bool OnPacketSend(CDataReader &reader) const;
};

extern CContextMenuManager g_ContextMenuManager;

} // namespace xuoassist

