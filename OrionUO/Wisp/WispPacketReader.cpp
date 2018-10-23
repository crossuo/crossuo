// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "stdafx.h"

namespace Wisp
{

CPacketReader::CPacketReader()
    : Wisp::CDataReader()
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
            UCHAR_LIST packet = parser->Read(this, offset);

            if (!packet.size())
                break;

            if (MaxPacketStackSize)
            {
                m_PacketsStack.push_back(packet);

                if ((int)m_PacketsStack.size() > MaxPacketStackSize)
                    m_PacketsStack.pop_front();
            }

            SetData((puchar)&packet[0], packet.size(), offset);
            OnPacket();
        }
    }
}

}; // namespace Wisp

