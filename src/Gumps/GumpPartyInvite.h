// SPDX-License-Identifier: MIT

#pragma once

#include "Gump.h"

class CGumpPartyInvite : public CGump
{
private:
    static constexpr int ID_PI_ACCEPT = 1;
    static constexpr int ID_PI_DECLINE = 2;

    uint32_t m_InviterNameSerial = 0;

public:
    CGumpPartyInvite(uint32_t inviter);
    virtual ~CGumpPartyInvite() = default;

    GUMP_BUTTON_EVENT_H override;
};
