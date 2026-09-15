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

CPacketPersonalLight::CPacketPersonalLight(uint32_t serial, uint8_t light)
    : CPacket(6)
{
    WriteUInt8(0x4E);
    WriteUInt32BE(serial);
    WriteUInt8(light);
}

CPacketToClientSystemPrint::CPacketToClientSystemPrint(const wstring &text, uint16_t color)
    : CPacket(1)
{
    auto size = 48 + text.length() * 2 + 2;
    Resize(size, true);

    WriteUInt8(0xAE);
    WriteUInt16BE(checked_cast<uint16_t>(size));
    WriteUInt32BE(0xFFFFFFFF);         //serial
    WriteUInt16BE(0);                  //graphic
    WriteUInt8(0);                     //mode
    WriteUInt16BE(color);              //color
    WriteUInt16BE(3);                  //font
    WriteUInt32BE(0x00000000);         //language
    WriteString("X:UO Assist", 29); //sender name
    WriteWString(text, text.length()); //content
}

CPacketToClientCharacterPrint::CPacketToClientCharacterPrint(
    uint32_t serial, const wstring &text, uint16_t color)
    : CPacket(1)
{
    auto size = 48 + text.length() * 2 + 2;
    Resize(size, true);

    WriteUInt8(0xAE);
    WriteUInt16BE(checked_cast<uint16_t>(size));
    WriteUInt32BE(serial);             //serial
    WriteUInt16BE(0);                  //graphic
    WriteUInt8(0);                     //mode
    WriteUInt16BE(color);              //color
    WriteUInt16BE(3);                  //font
    WriteUInt32BE(0x00000000);         //language
    WriteString("X:UO Assist", 29); //sender name
    WriteWString(text, text.length()); //content
}

CPacketToClientTextReplacePrintA::CPacketToClientTextReplacePrintA(
    const string &text,
    uint32_t serial,
    uint16_t graphic,
    uint8_t messageType,
    uint16_t color,
    uint16_t font)
    : CPacket(1)
{
    auto size = 44 + text.length() + 1;
    Resize(size, true);

    string name = "X:UO Assist";

    if (g_World != nullptr)
    {
        CGameObject *obj = g_World->FindWorldObject(serial);

        if (obj != nullptr && (obj->GetName().length() != 0))
        {
            name = obj->GetName();
        }
    }

    WriteUInt8(0x1C);
    WriteUInt16BE(checked_cast<uint16_t>(size));
    WriteUInt32BE(serial);               //serial
    WriteUInt16BE(graphic);              //graphic
    WriteUInt8(messageType);             //mode
    WriteUInt16BE(color);                //color
    WriteUInt16BE(font);                 //font
    WriteString(name, 29);               //sender name
    WriteString(text, text.length());    //content
}

CPacketToClientTextReplacePrintW::CPacketToClientTextReplacePrintW(
    const wstring &text,
    uint32_t serial,
    uint16_t graphic,
    uint8_t messageType,
    uint16_t color,
    uint16_t font,
    uint32_t lang)
    : CPacket(1)
{
    auto size = 48 + text.length() * 2 + 2;
    Resize(size, true);

    string name = "X:UO Assist";

    if (g_World != nullptr)
    {
        CGameObject *obj = g_World->FindWorldObject(serial);

        if (obj != nullptr && (obj->GetName().length() != 0))
        {
            name = obj->GetName();
        }
    }

    WriteUInt8(0xAE);
    WriteUInt16BE(checked_cast<uint16_t>(size));
    WriteUInt32BE(serial);               //serial
    WriteUInt16BE(graphic);              //graphic
    WriteUInt8(messageType);             //mode
    WriteUInt16BE(color);                //color
    WriteUInt16BE(font);                 //font
    WriteUInt32BE(lang);                 //language
    WriteString(name, 29);               //sender name
    WriteWString(text, text.length());   //content
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

CPacketStatusRequest::CPacketStatusRequest(uint32_t serial)
    : CPacket(10)
{
    WriteUInt8(0x34);
    WriteUInt32BE(0xEDEDEDED);
    WriteUInt8(4);
    WriteUInt32BE(serial);
}

CPacketClickRequest::CPacketClickRequest(uint32_t serial)
    : CPacket(5)
{
    WriteUInt8(0x09);
    WriteUInt32BE(serial);
}

CPacketDoubleClickRequest::CPacketDoubleClickRequest(uint32_t serial)
    : CPacket(5)
{
    WriteUInt8(0x06);
    WriteUInt32BE(serial);
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

CPacketHelpRequest::CPacketHelpRequest()
    : CPacket(258)
{
    WriteUInt8(0x9B);
}

CPacketChangeWarmode::CPacketChangeWarmode(uint8_t state)
    : CPacket(5)
{
    WriteUInt8(0x72);
    WriteUInt8(state);
    WriteUInt16BE(0x0032);
}

CPacketResend::CPacketResend()
    : CPacket(3)
{
    WriteUInt8(0x22);
}

CPacketEmoteAction::CPacketEmoteAction(const char *action)
    : CPacket(5 + strlen(action))
{
    WriteUInt8(0x12);
    WriteUInt16BE(checked_cast<uint16_t>(5 + strlen(action)));
    WriteUInt8(0xC7);
    WriteString(action);
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

CPacketPickupRequest::CPacketPickupRequest(uint32_t serial, uint16_t count)
    : CPacket(7)
{
    WriteUInt8(0x07);
    WriteUInt32BE(serial);
    WriteUInt16BE(count);
}

CPacketDropRequestOld::CPacketDropRequestOld(
    uint32_t serial, uint16_t x, uint16_t y, uint8_t z, uint32_t container)
    : CPacket(14)
{
    WriteUInt8(0x08);
    WriteUInt32BE(serial);
    WriteUInt16BE(x);
    WriteUInt16BE(y);
    WriteUInt8(z);
    WriteUInt32BE(container);
}

CPacketDropRequestNew::CPacketDropRequestNew(
    uint32_t serial, uint16_t x, uint16_t y, uint8_t z, uint8_t slot, uint32_t container)
    : CPacket(15)
{
    WriteUInt8(0x08);
    WriteUInt32BE(serial);
    WriteUInt16BE(x);
    WriteUInt16BE(y);
    WriteUInt8(z);
    WriteUInt8(slot);
    WriteUInt32BE(container);
}

CPacketEquipRequest::CPacketEquipRequest(uint32_t serial, uint8_t layer, uint32_t container)
    : CPacket(10)
{
    WriteUInt8(0x13);
    WriteUInt32BE(serial);
    WriteUInt8(layer);
    WriteUInt32BE(container);
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

CPacketGraphicEffect::CPacketGraphicEffect(
    uint16_t graphic, uint8_t duration, int16_t x, int16_t y, uint8_t z)
    : CPacket(28)
{
    WriteUInt8(0x70);
    WriteUInt8(2);             //EF_STAY_AT_POS
    WriteUInt32BE(0x00000000); //sourceSerial
    WriteUInt32BE(0x00000000); //destSerial
    WriteUInt16BE(graphic);
    WriteInt16BE(x); //src pos
    WriteInt16BE(y);
    WriteUInt8(z);
    WriteUInt16BE(0); //dest pos
    WriteUInt16BE(0);
    WriteUInt8(0);
    WriteUInt8(1);        //speed
    WriteUInt8(duration); //duration
    WriteUInt16BE(0);     //??
    WriteUInt8(0);        //fixedDirection
    WriteUInt8(0);        //explode
}

CPacketPartyAccept::CPacketPartyAccept(uint32_t serial)
    : CPacket(10)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000a);
    WriteUInt16BE(0x0006);
    WriteUInt8(0x08);
    WriteUInt32BE(serial);
}

CPacketPartyDecline::CPacketPartyDecline(uint32_t serial)
    : CPacket(10)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000a);
    WriteUInt16BE(0x0006);
    WriteUInt8(0x09);
    WriteUInt32BE(serial);
}

CPacketTradeClose::CPacketTradeClose(uint32_t id)
    : CPacket(17)
{
    WriteUInt8(0x6F);
    WriteUInt16BE(17);
    WriteUInt8(0x01);
    WriteUInt32BE(id);
}

CPacketTradeCheckState::CPacketTradeCheckState(uint32_t id, bool state)
    : CPacket(17)
{
    WriteUInt8(0x6F);
    WriteUInt16BE(17);
    WriteUInt8(0x02);
    WriteUInt32BE(id);
    WriteUInt32BE(state);
}

CPacketDyeData::CPacketDyeData(uint32_t serial)
    : CPacket(9)
{
    WriteUInt8(0x95);
    WriteUInt32BE(serial);
    WriteUInt16BE(0);
    WriteUInt16BE(0x0FAB);
}

CPacketUseCombatAbility::CPacketUseCombatAbility(uint8_t index)
    : CPacket(15)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000F);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0019);
    WriteUInt32BE(0x00000000);
    WriteUInt8(index);
    WriteUInt8(0x0A);
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

CPacketInvokeVirtureRequest::CPacketInvokeVirtureRequest(uint8_t id)
    : CPacket(6)
{
    WriteUInt8(0x12);
    WriteUInt16BE(0x0006);
    WriteUInt8(0xF4);
    WriteUInt8(id);
    WriteUInt8(0x00);
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

CPacketProfileRequest::CPacketProfileRequest(uint32_t serial)
    : CPacket(8)
{
    WriteUInt8(0xB8);
    WriteUInt16BE(0x0008);
    WriteUInt8(0);
    WriteUInt32BE(serial);
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

CPacketContextMenuRequest::CPacketContextMenuRequest(uint32_t serial)
    : CPacket(9)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x0009);
    WriteUInt16BE(0x0013);
    WriteUInt32BE(serial);
}

CPacketContextMenuSelection::CPacketContextMenuSelection(uint32_t serial, uint32_t index)
    : CPacket(11)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000B);
    WriteUInt16BE(0x0015);
    WriteUInt32BE(serial);
    WriteUInt16BE(checked_cast<uint16_t>(index));
}

CPacketTargetUseObject::CPacketTargetUseObject(
    uint32_t useObjectSerial, uint32_t targetObjectSerial)
    : CPacket(13)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000D);
    WriteUInt16BE(0x002C);
    WriteUInt32BE(useObjectSerial);
    WriteUInt32BE(targetObjectSerial);
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

CPacketConfirmWalk::CPacketConfirmWalk(uint8_t direction, uint8_t notoriety)
    : CPacket(3)
{
    WriteUInt8(0x22);
    WriteUInt8(direction);
    WriteUInt8(notoriety);
}

CPacketMapUOPartyInfo::CPacketMapUOPartyInfo()
    : CPacket(4)
{
    WriteUInt8(0xF0);
    WriteUInt16BE(4);
    WriteUInt8(0x00);
}

CPacketMapUOGuildInfo::CPacketMapUOGuildInfo()
    : CPacket(5)
{
    WriteUInt8(0xF0);
    WriteUInt16BE(5);
    WriteUInt8(0x01);
    WriteUInt8(0x01);
}

CPacketGuildMenuRequest::CPacketGuildMenuRequest()
    : CPacket(10)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000A);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0028);
    WriteUInt8(0x0A);
}

CPacketQuestMenuRequest::CPacketQuestMenuRequest()
    : CPacket(10)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000A);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0032);
    WriteUInt8(0x0A);
}

CPacketToggleGargoyleFlying::CPacketToggleGargoyleFlying()
    : CPacket(11)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000B);
    WriteUInt16BE(0x0032);
    WriteUInt16BE(0x0001);
    WriteUInt32BE(0x00000000);
}

CPacketCloseGenericGumpWithoutResponse::CPacketCloseGenericGumpWithoutResponse(
    CGump *gump, bool all)
    : CPacket(14)
{
    WriteUInt8(0xFC);
    WriteUInt16BE(0x000E);
    WriteUInt16BE(OCT_CLOSE_GENERIC_GUMP_WITHOUT_RESPONSE);
    WriteUInt32BE(gump->Serial);
    WriteUInt32BE(gump->ID);
    WriteUInt8(all ? 1 : 0);
}

CPacketSelectMenuInClient::CPacketSelectMenuInClient(uint32_t serial, uint32_t id, int32_t code)
    : CPacket(17)
{
    WriteUInt8(0xFC);
    WriteUInt16BE(0x0011);
    WriteUInt16BE(OCT_SELECT_MENU);
    WriteUInt32BE(serial);
    WriteUInt32BE(id);
    WriteInt32BE(code);
}

CPacketCastSpellRequest::CPacketCastSpellRequest(int32_t id)
    : CPacket(9)
{
    WriteUInt8(0xFC);
    WriteUInt16BE(0x0009);
    WriteUInt16BE(OCT_CAST_SPELL_REQUEST);
    WriteInt32BE(id);
}

CPacketUseSkillRequest::CPacketUseSkillRequest(int32_t id)
    : CPacket(9)
{
    WriteUInt8(0xFC);
    WriteUInt16BE(0x0009);
    WriteUInt16BE(OCT_USE_SKILL_REQUEST);
    WriteInt32BE(id);
}

CPacketDrawStatusbar::CPacketDrawStatusbar(uint32_t serial, int32_t x, int32_t y, bool minimized)
    : CPacket(18)
{
    WriteUInt8(0xFC);
    WriteUInt16BE(0x0012);
    WriteUInt16BE(OCT_DRAW_STATUSBAR);
    WriteUInt32BE(serial);
    WriteInt32BE(x);
    WriteInt32BE(y);
    WriteUInt8(minimized ? 1 : 0);
}

CPacketCloseStatusbar::CPacketCloseStatusbar(uint32_t serial)
    : CPacket(9)
{
    WriteUInt8(0xFC);
    WriteUInt16BE(0x0009);
    WriteUInt16BE(OCT_CLOSE_STATUSBAR);
    WriteUInt32BE(serial);
}

CPacketSecureTradeCheck::CPacketSecureTradeCheck(uint32_t id1, bool state)
    : CPacket(10)
{
    WriteUInt8(0xFC);
    WriteUInt16BE(0x000A);
    WriteUInt16BE(OCT_SECURE_TRADE_CHECK);
    WriteUInt32BE(id1);
    WriteUInt8(state ? 1 : 0);
}

CPacketSecureTradeClose::CPacketSecureTradeClose(uint32_t id1)
    : CPacket(9)
{
    WriteUInt8(0xFC);
    WriteUInt16BE(0x0009);
    WriteUInt16BE(OCT_SECURE_TRADE_CLOSE);
    WriteUInt32BE(id1);
}

CPacketUnicodeSpeechRequest::CPacketUnicodeSpeechRequest(uint16_t color, const wstring &text)
    : CPacket(1)
{
    auto size = 7 + text.length() * 2 + 2;
    Resize(size, true);

    WriteUInt8(0xFC);
    WriteUInt16BE(checked_cast<uint16_t>(size));
    WriteUInt16BE(OCT_UNICODE_SPEECH_REQUEST);
    WriteUInt16BE(color);
    WriteWString(text);
}

CPacketRenameMountRequest::CPacketRenameMountRequest(uint32_t serial, const string &name)
    : CPacket(1)
{
    auto size = 9 + name.length() + 1;
    Resize(size, true);

    WriteUInt8(0xFC);
    WriteUInt16BE(checked_cast<uint16_t>(size));
    WriteUInt16BE(OCT_RENAME_MOUNT_REQUEST);
    WriteUInt32BE(serial);
    WriteString(name);
}

CPacketReconnect::CPacketReconnect()
    : CPacket(5)
{
    WriteUInt8(0xFC);
    WriteUInt16BE(0x0005);
    WriteUInt16BE(OCT_RECONNECT);
}

CPacketPlayMacro::CPacketPlayMacro(
    const vector<string> &actionList, const vector<string> &subActionList)
    : CPacket(1)
{
    int size = 7;
    for (const string &s : actionList)
    {
        size += s.length() + 1;
    }

    for (const string &s : subActionList)
    {
        size += s.length() + 1;
    }

    Resize(size, true);
    int count = actionList.size();

    WriteUInt8(0xFC);
    WriteUInt16BE(checked_cast<uint16_t>(size));
    WriteUInt16BE(OCT_PLAY_MACRO);
    WriteUInt16BE(checked_cast<uint16_t>(count));

    for (int i = 0; i < count; i++)
    {
        WriteString(actionList[i]);
        WriteString(subActionList[i]);
    }
}

CPacketMovePaperdoll::CPacketMovePaperdoll(uint32_t serial, int32_t x, int32_t y)
    : CPacket(14)
{
    WriteUInt8(0xFC);
    WriteUInt16BE(14);
    WriteUInt16BE(OCT_MOVE_PAPERDOLL);
    WriteUInt32BE(serial);
    WriteInt16BE(x);
    WriteInt16BE(y);
    WriteUInt8(0);
}

CPacketUseAbilityRequest::CPacketUseAbilityRequest(int32_t index)
    : CPacket(6)
{
    WriteUInt8(0xFC);
    WriteUInt16BE(6);
    WriteUInt16BE(OCT_USE_ABILITY);
    WriteUInt8(index);
}

CPacketOpenDoorRequest::CPacketOpenDoorRequest()
    : CPacket(5)
{
    WriteUInt8(0xFC);
    WriteUInt16BE(0x0005);
    WriteUInt16BE(OCT_OPEN_DOOR);
}

} // namespace xuoassist
