// MIT License

#ifndef WISP_PCKT_H
#define WISP_PCKT_H

namespace Wisp
{
class CPacketMessage
{
    bool BigEndian = false;

protected:
    vector<uint8_t> m_Data;

public:
    CPacketMessage(bool bigEndian = true);
    CPacketMessage(uint8_t *data, int dataSizeconst, bool bigEndian = true);
    CPacketMessage(const vector<uint8_t> &dataconst, bool bigEndian = true);
    virtual ~CPacketMessage();

    void Append(uint8_t *data, int dataSize);
    void Append(const vector<uint8_t> &data);

    void Clear() { m_Data.clear(); }

    vector<uint8_t> Read(class CPacketReader *reader, int &dataOffset);
};

}; // namespace Wisp

#endif // WISP_PCKT_H
