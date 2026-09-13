// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (assist/packets.h). The builders are
// protocol-critical and byte-identical to the originals; do not "improve"
// them. Changes vs the original app (plan §4.1): the only framework bits were the `emit`
// marshaling calls in CPacket::SendClient/SendServer — in-process they go
// straight to the client (SendClient injects into the client's receive path,
// SendServer goes to g_ConnectionManager.Send, mirroring the UOMSG_*
// round-trip the external plugin used, minus the SDL event hop).
// Lists are std::vector, ../managers/gumpmanager.h -> the ported CGump.

#pragma once

#include "global.h"
#include "../model/datawriter.h"
#include "../model/commonitems/gump.h"

namespace xuoassist {

#define DEFINE_PACKET(name, ...)                                                                \
    class CPacket##name : public CPacket                                                           \
    {                                                                                              \
    public:                                                                                        \
        CPacket##name(__VA_ARGS__);                                                                \
    }

class CPacket : public CDataWriter
{
public:
    CPacket(size_t size, bool autoResize = false);

    void SendClient();
    void SendServer();
};

DEFINE_PACKET(Light, uint8_t light);
DEFINE_PACKET(PersonalLight, uint32_t serial, uint8_t light);
DEFINE_PACKET(ToClientSystemPrint, const wstring &text, uint16_t color);
DEFINE_PACKET(ToClientCharacterPrint, uint32_t serial, const wstring &text, uint16_t color);
DEFINE_PACKET(
    ToClientTextReplacePrintA,
    const string &text,
    uint32_t serial,
    uint16_t graphic,
    uint8_t messageType,
    uint16_t color,
    uint16_t font);
DEFINE_PACKET(
    ToClientTextReplacePrintW,
    const wstring &text,
    uint32_t serial,
    uint16_t graphic,
    uint8_t messageType,
    uint16_t color,
    uint16_t font,
    uint32_t lang);
DEFINE_PACKET(ToClientTargetRequest);
DEFINE_PACKET(StatusRequest, uint32_t serial);
DEFINE_PACKET(ClickRequest, uint32_t serial);
DEFINE_PACKET(DoubleClickRequest, uint32_t serial);
DEFINE_PACKET(AttackRequest, uint32_t serial);
DEFINE_PACKET(Weather, const WEATHER_DATA &data);
DEFINE_PACKET(Season, const SEASON_DATA &data);
DEFINE_PACKET(HelpRequest);
DEFINE_PACKET(ChangeWarmode, uint8_t state);
DEFINE_PACKET(Resend);
DEFINE_PACKET(EmoteAction, const char *action);
DEFINE_PACKET(HideObject, uint32_t serial);
DEFINE_PACKET(PlaySound, uint16_t index, int16_t x, int16_t y, int8_t z);
DEFINE_PACKET(PickupRequest, uint32_t serial, uint16_t count);
DEFINE_PACKET(
    DropRequestOld, uint32_t serial, uint16_t x, uint16_t y, uint8_t z, uint32_t container);
DEFINE_PACKET(
    DropRequestNew,
    uint32_t serial,
    uint16_t x,
    uint16_t y,
    uint8_t z,
    uint8_t slot,
    uint32_t container);
DEFINE_PACKET(EquipRequest, uint32_t serial, uint8_t layer, uint32_t container);
DEFINE_PACKET(QuestArrowOld, bool enabled, int16_t x, int16_t y);
DEFINE_PACKET(QuestArrowNew, bool enabled, int16_t x, int16_t y);
DEFINE_PACKET(
    MenuResponse, uint32_t serial, uint32_t id, int32_t code, uint16_t graphic, uint16_t color);
DEFINE_PACKET(GrayMenuResponse, uint32_t serial, uint32_t id, int32_t code);
DEFINE_PACKET(GraphicEffect, uint16_t graphic, uint8_t duration, int16_t x, int16_t y, uint8_t z);
DEFINE_PACKET(PartyAccept, uint32_t serial);
DEFINE_PACKET(PartyDecline, uint32_t serial);
DEFINE_PACKET(TradeClose, uint32_t id);
DEFINE_PACKET(TradeCheckState, uint32_t id, bool state);
DEFINE_PACKET(DyeData, uint32_t serial);
DEFINE_PACKET(UseCombatAbility, uint8_t index);
DEFINE_PACKET(WrestlingStun);
DEFINE_PACKET(WrestlingDisarm);
DEFINE_PACKET(CancelTarget);
DEFINE_PACKET(InvokeVirtureRequest, uint8_t id);
DEFINE_PACKET(GumpResponse, CGump *gump, int32_t code);
DEFINE_PACKET(TargetCancel, uint8_t type, uint32_t targetSerial, uint8_t targetType);
DEFINE_PACKET(
    TargetObject, uint8_t type, uint32_t targetSerial, uint8_t targetType, uint32_t serial);
DEFINE_PACKET(
    TargetTile,
    uint8_t type,
    uint32_t targetSerial,
    uint8_t targetType,
    uint16_t graphic,
    uint16_t x,
    uint16_t y,
    uint8_t z);
DEFINE_PACKET(ProfileRequest, uint32_t serial);
DEFINE_PACKET(ASCIIPromptResponse, uint32_t serial, uint32_t id, const string &text, bool cancel);
DEFINE_PACKET(
    UnicodePromptResponse,
    uint32_t serial,
    uint32_t id,
    const wstring &text,
    const string &lang,
    bool cancel);
DEFINE_PACKET(BuyRequest, uint32_t serial, const vector<std::pair<uint, int>> &list);
DEFINE_PACKET(SellRequest, uint32_t serial, const vector<std::pair<uint, int>> &list);
DEFINE_PACKET(LogOut);
DEFINE_PACKET(ClosePaperdoll, uint32_t serial);
DEFINE_PACKET(ContextMenuRequest, uint32_t serial);
DEFINE_PACKET(ContextMenuSelection, uint32_t serial, uint32_t index);
DEFINE_PACKET(TargetUseObject, uint32_t useObjectSerial, uint32_t targetObjectSerial);
DEFINE_PACKET(TargetCastSpell, uint16_t spell, uint32_t targetObjectSerial);
DEFINE_PACKET(TargetUseSkill, uint16_t skill, uint32_t targetObjectSerial);
DEFINE_PACKET(ClientViewRange, uchar range);
DEFINE_PACKET(ConfirmWalk, uint8_t direction, uint8_t notoriety);
DEFINE_PACKET(MapUOPartyInfo);
DEFINE_PACKET(MapUOGuildInfo);
DEFINE_PACKET(GuildMenuRequest);
DEFINE_PACKET(QuestMenuRequest);
DEFINE_PACKET(ToggleGargoyleFlying);

DEFINE_PACKET(CloseGenericGumpWithoutResponse, CGump *gump, bool all);
DEFINE_PACKET(SelectMenuInClient, uint32_t serial, uint32_t id, int32_t code);
DEFINE_PACKET(CastSpellRequest, int32_t id);
DEFINE_PACKET(UseSkillRequest, int32_t id);
DEFINE_PACKET(DrawStatusbar, uint32_t serial, int32_t x, int32_t y, bool minimized);
DEFINE_PACKET(CloseStatusbar, uint32_t serial);
DEFINE_PACKET(SecureTradeCheck, uint32_t id1, bool state);
DEFINE_PACKET(SecureTradeClose, uint32_t id1);
DEFINE_PACKET(UnicodeSpeechRequest, uint16_t color, const wstring &text);
DEFINE_PACKET(RenameMountRequest, uint32_t serial, const string &name);
DEFINE_PACKET(Reconnect);
DEFINE_PACKET(PlayMacro, const vector<string> &actionList, const vector<string> &subActionList);
DEFINE_PACKET(MovePaperdoll, uint32_t serial, int32_t x, int32_t y);
DEFINE_PACKET(UseAbilityRequest, int32_t index);
DEFINE_PACKET(OpenDoorRequest);

} // namespace xuoassist

