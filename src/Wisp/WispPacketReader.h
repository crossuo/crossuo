// MIT License

#ifndef WISP_PR_H
#define WISP_PR_H

#include "../CrossPCH.h" // REMOVE
#include <xuocore/mappedfile.h>

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
