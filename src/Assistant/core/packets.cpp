// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (assist/packets.cpp). Builder bodies are byte
// identical to the originals (protocol-critical; plan "must-follow" rule).
// Only the transport changed: the original app's `emit g_Assistant.signal_*`
// cross-thread marshaling becomes direct client calls (plan §4.1):
//   SendClient -> client receive injection (queued to the client's packet loop)
//   SendServer -> g_ConnectionManager.Send
// String split/toInt/wide-conversion helpers replaced by local std equivalents.

// SDL types must come first: xuocore/plugininterface.h maps its event structs
// onto SDL's when SDL headers are visible (USING_SDL_STRUCTS), which is what
// makes the client's PacketManager.h (Wisp/Platform) coexist with it here.
#include <SDL_wrapper.h>
#include "packets.h"
#include "target.h" // g_Target.SetRecvAssistData (target cursor the assistant requested)
#include <string.h>
#include <stdlib.h>
#include <common/str.h> // wstr_from
#include "Managers/PacketManager.h"      // ::g_PacketManager (client, inject)
#include "Managers/ConnectionManager.h" // ::g_ConnectionManager (client, send)
#include "../core/world.h"
#include "Managers/ConnectionManager.h" // ::g_ConnectionManager (client, send)

namespace xuoassist {

CPacket::CPacket(size_t size, bool autoResize)
    : CDataWriter(size, autoResize)
{
}

void CPacket::SendClient()
{
    if (!m_Data.empty())
    {
        // client-bound injection: queued for the client's packet loop
        ::g_PacketManager.SaveAssistantReceivePacket(&m_Data[0], (int)m_Data.size());
    }
}

void CPacket::SendServer()
{
    if (!m_Data.empty())
    {
        // direct wire send; the assistant's own send tap does not see it
        ::g_ConnectionManager.Send(&m_Data[0], (int)m_Data.size());
    }
}

// split on spaces, skipping empty parts
static vector<string> split_skip_empty(const string &text)
{
    vector<string> result;
    size_t start = 0;
    while (start <= text.length())
    {
        size_t pos = text.find(' ', start);
        if (pos == string::npos)
        {
            if (start < text.length())
            {
                result.push_back(text.substr(start));
            }
            break;
        }
        if (pos > start)
        {
            result.push_back(text.substr(start, pos - start));
        }
        start = pos + 1;
    }
    return result;
}

CPacketLight::CPacketLight(uint8_t light)
    : CPacket(2)
{
    WriteUInt8(0x4F);
    WriteUInt8(light);
}

CPacketToClientTargetRequest::CPacketToClientTargetRequest()
    : CPacket(19)
{
    WriteUInt8(0x6C);
    Move(1);
    WriteUInt32BE(0x0000601A);
    WriteUInt32BE(0x00010000);

    // The original build handed the fresh packet to
    // g_Target.SetRecvAssistData(CDataReader(...)) here (assist/packets.cpp,
    // Phase 1c): the packet bytes above are the target cursor the assistant
    // itself requested, so the assistant target state is set eagerly (the
    // cursor fields are read back from the packet, offset 1 skipped).
    vector<uint8_t> packet = Data();
    CDataReader reader(&packet[0], packet.size());
    reader.Move(1);
    g_Target.SetRecvAssistData(reader);
}

CPacketWeather::CPacketWeather(const WEATHER_DATA &data)
    : CPacket(4)
{
    WriteUInt8(0x65);
    WriteUInt8(data.Type);
    WriteUInt8(data.Count);
    WriteUInt8(data.Temperature);
}

CPacketSeason::CPacketSeason(const SEASON_DATA &data)
    : CPacket(3)
{
    WriteUInt8(0xBC);
    WriteUInt8(data.Index);
    WriteUInt8(data.Music);
}

CPacketHideObject::CPacketHideObject(uint32_t serial)
    : CPacket(5)
{
    WriteUInt8(0x1D);
    WriteUInt32BE(serial);
}

CPacketPlaySound::CPacketPlaySound(uint16_t index, int16_t x, int16_t y, int8_t z)
    : CPacket(12)
{
    WriteUInt8(0x54);
    WriteUInt8(0x01);
    WriteUInt16BE(index);
    WriteUInt16BE(0x00);
    WriteInt16BE(x);
    WriteInt16BE(y);
    WriteInt8(z);
}

CPacketQuestArrowOld::CPacketQuestArrowOld(bool enabled, int16_t x, int16_t y)
    : CPacket(6)
{
    WriteUInt8(0xBA);
    WriteUInt8(enabled);
    WriteInt16BE(x);
    WriteInt16BE(y);
}

CPacketQuestArrowNew::CPacketQuestArrowNew(bool enabled, int16_t x, int16_t y)
    : CPacket(10)
{
    WriteUInt8(0xBA);
    WriteUInt8(enabled);
    WriteInt16BE(x);
    WriteInt16BE(y);
    WriteUInt32BE(0);
}

CPacketMenuResponse::CPacketMenuResponse(
    uint32_t serial, uint32_t id, int32_t code, uint16_t graphic, uint16_t color)
    : CPacket(13)
{
    WriteUInt8(0x7D);
    WriteUInt32BE(serial);
    WriteUInt16BE(id);
    WriteUInt16BE(code);
    WriteUInt16BE(graphic);
    WriteUInt16BE(color);
}

CPacketGrayMenuResponse::CPacketGrayMenuResponse(uint32_t serial, uint32_t id, int32_t code)
    : CPacket(13)
{
    WriteUInt8(0x7D);
    WriteUInt32BE(serial);
    WriteUInt16BE(id);
    WriteUInt16BE(code);
}

CPacketDyeData::CPacketDyeData(uint32_t serial)
    : CPacket(9)
{
    WriteUInt8(0x95);
    WriteUInt32BE(serial);
    WriteUInt16BE(0);
    WriteUInt16BE(0x0FAB);
}

CPacketWrestlingStun::CPacketWrestlingStun()
    : CPacket(5)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x05);
    WriteUInt16BE(0x09);
}

CPacketWrestlingDisarm::CPacketWrestlingDisarm()
    : CPacket(5)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x05);
    WriteUInt16BE(0x0A);
}

CPacketCancelTarget::CPacketCancelTarget()
    : CPacket(19)
{
    WriteUInt8(0x6C);
    WriteUInt8(0x00);
    WriteUInt32BE(0x00000000);
    WriteUInt8(0x03);
}

CPacketGumpResponse::CPacketGumpResponse(CGump *gump, int32_t code)
    : CPacket(1)
{
    int switchesCount = 0;
    int textLinesCount = 0;
    int textLinesLength = 0;

    for (CGumpItem &item : gump->m_Items)
    {
        switch (item.Type)
        {
            case GIT_CHECKBOX:
            case GIT_RADIO:
            {
                vector<string> commands = split_skip_empty(item.Command);

                if (commands.size() >= 7 && (atoi(commands[5].c_str()) != 0))
                {
                    switchesCount++;
                }

                break;
            }
            case GIT_TEXTENTRY:
            case GIT_TEXTENTRYLIMITED:
            {
                vector<string> commands = split_skip_empty(item.Command);

                if (commands.size() >= 8)
                {
                    uint textIndex = (uint)strtoul(commands[7].c_str(), nullptr, 10);

                    if (textIndex < (uint)gump->m_Text.size())
                    {
                        textLinesLength += gump->m_Text[textIndex].length() * 2;
                        textLinesCount++;
                    }
                }

                break;
            }
            default:
                break;
        }
    }

    int size = 19 + (switchesCount * 4) + 4 + ((textLinesCount * 4) + textLinesLength);
    Resize(size, true);

    WriteUInt8(0xB1);
    WriteUInt16BE(checked_cast<uint16_t>(size));
    WriteUInt32BE(gump->Serial);
    WriteUInt32BE(gump->ID);
    WriteUInt32BE(code);
    WriteUInt32BE(switchesCount);

    for (CGumpItem &item : gump->m_Items)
    {
        if (item.Type == GIT_CHECKBOX || item.Type == GIT_RADIO)
        {
            vector<string> commands = split_skip_empty(item.Command);

            if (commands.size() >= 7 && (atoi(commands[5].c_str()) != 0))
            {
                WriteUInt32BE(atoi(commands[6].c_str()));
            }
        }
    }

    WriteUInt32BE(textLinesCount);

    for (CGumpItem &item : gump->m_Items)
    {
        if (item.Type == GIT_TEXTENTRY || item.Type == GIT_TEXTENTRYLIMITED)
        {
            vector<string> commands = split_skip_empty(item.Command);

            if (commands.size() >= 8)
            {
                uint textIndex = (uint)strtoul(commands[7].c_str(), nullptr, 10);

                if (textIndex < (uint)gump->m_Text.size())
                {
                    string &line = gump->m_Text[textIndex];
                    wstring wline = wstr_from(line);

                    WriteUInt16BE(atoi(commands[6].c_str()));
                    WriteUInt16BE((uint16_t)wline.length());
                    WriteWString(wline, wline.length(), true, false);
                }
            }
        }
    }
}

CPacketTargetCancel::CPacketTargetCancel(uint8_t type, uint32_t targetSerial, uint8_t targetType)
    : CPacket(19)
{
    WriteUInt8(0x6C);
    WriteUInt8(type);
    WriteUInt32BE(targetSerial);
    WriteUInt8(targetType);
    WriteUInt32BE(0);
    WriteUInt16BE(0xFFFF);
    WriteUInt16BE(0xFFFF);
    WriteUInt16BE(0);
    WriteUInt16BE(0);
}

CPacketTargetObject::CPacketTargetObject(
    uint8_t type, uint32_t targetSerial, uint8_t targetType, uint32_t serial)
    : CPacket(19)
{
    WriteUInt8(0x6C);
    WriteUInt8(type);
    WriteUInt32BE(targetSerial);
    WriteUInt8(targetType);
    WriteUInt32BE(serial);

    CGameObject *obj = (g_World != nullptr ? g_World->FindWorldObject(serial) : nullptr);

    if (obj != nullptr)
    {
        WriteUInt16BE(obj->GetX());
        WriteUInt16BE(obj->GetY());
        WriteUInt16BE(obj->GetZ());
        WriteUInt16BE(obj->Graphic);
    }
}

CPacketTargetTile::CPacketTargetTile(
    uint8_t type,
    uint32_t targetSerial,
    uint8_t targetType,
    uint16_t graphic,
    uint16_t x,
    uint16_t y,
    uint8_t z)
    : CPacket(19)
{
    WriteUInt8(0x6C);
    WriteUInt8(type);
    WriteUInt32BE(targetSerial);
    WriteUInt8(targetType);
    WriteUInt32BE(0);
    WriteUInt16BE(x);
    WriteUInt16BE(y);
    WriteUInt16BE(z);
    WriteUInt16BE(graphic);
}

CPacketASCIIPromptResponse::CPacketASCIIPromptResponse(
    uint32_t serial, uint32_t id, const string &text, bool cancel)
    : CPacket(1)
{
    int size = 15 + text.length() + 1;
    Resize(size, true);

    WriteUInt8(0x9A);
    WriteUInt16BE(size);
    WriteUInt32BE(serial);
    WriteUInt32BE(id);
    WriteUInt32BE((int)(!cancel));
    WriteString(text);
}

CPacketUnicodePromptResponse::CPacketUnicodePromptResponse(
    uint32_t serial, uint32_t id, const wstring &text, const string &lang, bool cancel)
    : CPacket(1)
{
    int size = 19 + (text.length() * 2);
    Resize(size, true);

    WriteUInt8(0xC2);
    WriteUInt16BE(size);
    WriteUInt32BE(serial);
    WriteUInt32BE(id);
    WriteUInt32BE(uint32_t(!cancel));
    WriteString(lang, 4, false);

    WriteWString(text, text.length(), false, false);
}

CPacketBuyRequest::CPacketBuyRequest(uint32_t serial, const vector<std::pair<uint, int>> &list)
    : CPacket(1)
{
    auto size = 8 + (list.size() * 7);
    Resize(size, true);

    WriteUInt8(0x3B);
    WriteUInt16BE(size);
    WriteUInt32BE(serial);

    if (!list.empty())
    {
        WriteUInt8(0x02);
        for (const auto &item : list)
        {
            WriteUInt8(0x1A);
            WriteUInt32BE(item.first);
            WriteUInt16BE(item.second);
        }
    }
    else
    {
        WriteUInt8(0x00);
    }
}

CPacketSellRequest::CPacketSellRequest(uint32_t serial, const vector<std::pair<uint, int>> &list)
    : CPacket(1)
{
    auto count = list.size();
    auto size = 9 + (count * 6);
    Resize(size, true);

    WriteUInt8(0x9F);
    WriteUInt16BE(checked_cast<uint16_t>(size));
    WriteUInt32BE(serial);
    WriteUInt16BE(checked_cast<uint16_t>(count));

    if (count != 0)
    {
        for (const auto &item : list)
        {
            WriteUInt32BE(item.first);
            WriteUInt16BE(item.second);
        }
    }
}

CPacketLogOut::CPacketLogOut()
    : CPacket(2)
{
    WriteUInt8(0xD1);
    WriteUInt8(0x00);
}

CPacketClosePaperdoll::CPacketClosePaperdoll(uint32_t serial)
    : CPacket(13)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(13);
    WriteUInt16BE(0x0016);
    WriteUInt32BE(1);
    WriteUInt32BE(serial);
}

CPacketTargetCastSpell::CPacketTargetCastSpell(uint16_t spell, uint32_t targetObjectSerial)
    : CPacket(11)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000B);
    WriteUInt16BE(0x002D);
    WriteUInt16BE(spell);
    WriteUInt32BE(targetObjectSerial);
}

CPacketTargetUseSkill::CPacketTargetUseSkill(uint16_t skill, uint32_t targetObjectSerial)
    : CPacket(11)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000B);
    WriteUInt16BE(0x002E);
    WriteUInt16BE(skill);
    WriteUInt32BE(targetObjectSerial);
}

CPacketClientViewRange::CPacketClientViewRange(uchar range)
    : CPacket(2)
{
    WriteUInt8(0xC8);

    if (range < 5)
    {
        range = 5;
    }
    else if (range > 24)
    {
        range = 24;
    }

    WriteUInt8(range);
}

} // namespace xuoassist
