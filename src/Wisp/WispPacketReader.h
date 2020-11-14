// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef WISP_PR_H
#define WISP_PR_H

#include <xuocore/mappedfile.h>
#include <vector>
#include <deque>
#include <stdint.h>

namespace Wisp
{
class CPacketReader : public CDataReader
{
    int MaxPacketStackSize = 5;

public:
    CPacketReader();
    virtual ~CPacketReader();

    void Read(class CConnection *connection);

    virtual int GetPacketSize(const std::vector<uint8_t> &packet, int &offsetToSize) { return 0; }

protected:
    virtual void OnPacket() {}

    virtual void OnReadFailed() {}

public:
    std::deque<std::vector<uint8_t>> m_PacketsStack;
};

}; // namespace Wisp

#endif // WISP_PR_H
