// MIT License

namespace Wisp
{
CPacketReader::CPacketReader()

{
}

CPacketReader::~CPacketReader()
{
}

void CPacketReader::Read(class CConnection *connection)
{
    DEBUG_TRACE_FUNCTION;
    if (connection->ReadyRead())
    {
        if (!connection->Read())
        {
            OnReadFailed();
            return;
        }

        Wisp::CPacketMessage *parser = connection->m_MessageParser;

        while (true)
        {
            int offset = 0;
            vector<uint8_t> packet = parser->Read(this, offset);

            if (packet.empty())
            {
                break;
            }

            if (MaxPacketStackSize != 0)
            {
                m_PacketsStack.push_back(packet);

                if ((int)m_PacketsStack.size() > MaxPacketStackSize)
                {
                    m_PacketsStack.pop_front();
                }
            }

            SetData((uint8_t *)&packet[0], packet.size(), offset);
            OnPacket();
        }
    }
}

}; // namespace Wisp
