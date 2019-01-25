// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CPacket : public Wisp::CDataWriter
{
public:
    CPacket(size_t size, bool autoResize = false);

    void Send();
};

class CPacketFirstLogin : public CPacket
{
public:
    CPacketFirstLogin();
};

class CPacketSelectServer : public CPacket
{
public:
    CPacketSelectServer(uint8_t index);
};

class CPacketSecondLogin : public CPacket
{
public:
    CPacketSecondLogin();
};

class CPacketCreateCharacter : public CPacket
{
public:
    CPacketCreateCharacter(const string &name);
};

class CPacketDeleteCharacter : public CPacket
{
public:
    CPacketDeleteCharacter(int charIndex);
};

class CPacketSelectCharacter : public CPacket
{
public:
    CPacketSelectCharacter(int index, const string &name);
};

class CPacketPickupRequest : public CPacket
{
public:
    CPacketPickupRequest(uint32_t serial, uint16_t count);
};

class CPacketDropRequestOld : public CPacket
{
public:
    CPacketDropRequestOld(uint32_t serial, uint16_t x, uint16_t y, char z, uint32_t container);
};

class CPacketDropRequestNew : public CPacket
{
public:
    CPacketDropRequestNew(
        uint32_t serial, uint16_t x, uint16_t y, char z, uint8_t slot, uint32_t container);
};

class CPacketEquipRequest : public CPacket
{
public:
    CPacketEquipRequest(uint32_t serial, uint8_t layer, uint32_t container);
};

class CPacketChangeWarmode : public CPacket
{
public:
    CPacketChangeWarmode(uint8_t state);
};

class CPacketHelpRequest : public CPacket
{
public:
    CPacketHelpRequest();
};

class CPacketStatusRequest : public CPacket
{
public:
    CPacketStatusRequest(uint32_t serial);
};

class CPacketSkillsRequest : public CPacket
{
public:
    CPacketSkillsRequest(uint32_t serial);
};

class CPacketSkillsStatusChangeRequest : public CPacket
{
public:
    CPacketSkillsStatusChangeRequest(uint8_t skill, uint8_t state);
};

class CPacketClickRequest : public CPacket
{
public:
    CPacketClickRequest(uint32_t serial);
};

class CPacketDoubleClickRequest : public CPacket
{
public:
    CPacketDoubleClickRequest(uint32_t serial);
};

class CPacketAttackRequest : public CPacket
{
public:
    CPacketAttackRequest(uint32_t serial);
};

class CPacketClientVersion : public CPacket
{
public:
    CPacketClientVersion(const string &version);
};

class CPacketASCIISpeechRequest : public CPacket
{
public:
    CPacketASCIISpeechRequest(const char *text, SPEECH_TYPE type, uint16_t font, uint16_t color);
};

class CPacketUnicodeSpeechRequest : public CPacket
{
public:
    CPacketUnicodeSpeechRequest(
        const wchar_t *text, SPEECH_TYPE type, uint16_t font, uint16_t color, uint8_t *language);
};

class CPacketCastSpell : public CPacket
{
public:
    CPacketCastSpell(int index);
};

class CPacketCastSpellFromBook : public CPacket
{
public:
    CPacketCastSpellFromBook(int index, uint32_t serial);
};

class CPacketUseSkill : public CPacket
{
public:
    CPacketUseSkill(int index);
};

class CPacketOpenDoor : public CPacket
{
public:
    CPacketOpenDoor();
};

class CPacketOpenSpellbook : public CPacket
{
public:
    CPacketOpenSpellbook(SPELLBOOK_TYPE type);
};

class CPacketEmoteAction : public CPacket
{
public:
    CPacketEmoteAction(const char *action);
};

class CPacketGumpResponse : public CPacket
{
public:
    CPacketGumpResponse(class CGumpGeneric *gump, int code);
};

class CPacketVirtueGumpResponse : public CPacket
{
public:
    CPacketVirtueGumpResponse(class CGump *gump, int code);
};

class CPacketMenuResponse : public CPacket
{
public:
    CPacketMenuResponse(class CGump *gump, int code);
};

class CPacketGrayMenuResponse : public CPacket
{
public:
    CPacketGrayMenuResponse(class CGump *gump, int code);
};

class CPacketTradeResponse : public CPacket
{
public:
    CPacketTradeResponse(class CGumpSecureTrading *gump, int code);
};

class CPacketLogoutNotification : public CPacket
{
public:
    CPacketLogoutNotification();
};

class CPacketTextEntryDialogResponse : public CPacket
{
public:
    CPacketTextEntryDialogResponse(
        class CGumpTextEntryDialog *gump, class CEntryText *entry, bool code);
};

class CPacketRenameRequest : public CPacket
{
public:
    CPacketRenameRequest(uint32_t serial, const string &newName);
};

class CPacketTipRequest : public CPacket
{
public:
    CPacketTipRequest(uint16_t id, uint8_t flag);
};

class CPacketASCIIPromptResponse : public CPacket
{
public:
    CPacketASCIIPromptResponse(const char *text, size_t len, bool cancel);
};

class CPacketUnicodePromptResponse : public CPacket
{
public:
    CPacketUnicodePromptResponse(const wchar_t *text, size_t len, const string &lang, bool cancel);
};

class CPacketDyeDataResponse : public CPacket
{
public:
    CPacketDyeDataResponse(uint32_t serial, uint16_t graphic, uint16_t color);
};

class CPacketProfileRequest : public CPacket
{
public:
    CPacketProfileRequest(uint32_t serial);
};

class CPacketProfileUpdate : public CPacket
{
public:
    CPacketProfileUpdate(uint32_t serial, const wchar_t *text, size_t len);
};

class CPacketCloseStatusbarGump : public CPacket
{
public:
    CPacketCloseStatusbarGump(uint32_t serial);
};

class CPacketPartyInviteRequest : public CPacket
{
public:
    CPacketPartyInviteRequest();
};

class CPacketPartyRemoveRequest : public CPacket
{
public:
    CPacketPartyRemoveRequest(uint32_t serial);
};

class CPacketPartyChangeLootTypeRequest : public CPacket
{
public:
    CPacketPartyChangeLootTypeRequest(uint8_t type);
};

class CPacketPartyAccept : public CPacket
{
public:
    CPacketPartyAccept(uint32_t serial);
};

class CPacketPartyDecline : public CPacket
{
public:
    CPacketPartyDecline(uint32_t serial);
};

class CPacketPartyMessage : public CPacket
{
public:
    CPacketPartyMessage(const wchar_t *text, size_t len, uint32_t serial = 0x00000000);
};

class CPacketGameWindowSize : public CPacket
{
public:
    CPacketGameWindowSize();
};

class CPacketClientViewRange : public CPacket
{
public:
    CPacketClientViewRange(uint8_t range);
};

class CPacketBulletinBoardRequestMessage : public CPacket
{
public:
    CPacketBulletinBoardRequestMessage(uint32_t serial, uint32_t msgSerial);
};

class CPacketBulletinBoardRequestMessageSummary : public CPacket
{
public:
    CPacketBulletinBoardRequestMessageSummary(uint32_t serial, uint32_t msgSerial);
};

class CPacketBulletinBoardPostMessage : public CPacket
{
public:
    CPacketBulletinBoardPostMessage(
        uint32_t serial, uint32_t replySerial, const char *subject, const char *message);
};

class CPacketBulletinBoardRemoveMessage : public CPacket
{
public:
    CPacketBulletinBoardRemoveMessage(uint32_t serial, uint32_t msgSerial);
};

class CPacketAssistVersion : public CPacket
{
public:
    CPacketAssistVersion(uint32_t version, const string &clientVersion);
};

class CPacketRazorAnswer : public CPacket
{
public:
    CPacketRazorAnswer();
};

class CPacketLanguage : public CPacket
{
public:
    CPacketLanguage(const string &lang);
};

class CPacketClientType : public CPacket
{
public:
    CPacketClientType();
};

class CPacketRequestPopupMenu : public CPacket
{
public:
    CPacketRequestPopupMenu(uint32_t serial);
};

class CPacketPopupMenuSelection : public CPacket
{
public:
    CPacketPopupMenuSelection(uint32_t serial, uint16_t menuID);
};

class CPacketOpenChat : public CPacket
{
public:
    CPacketOpenChat(const wstring &name);
};

class CPacketMapMessage : public CPacket
{
public:
    CPacketMapMessage(
        uint32_t serial, MAP_MESSAGE action, uint8_t pin = 0, short x = -24, short y = -31);
};

class CPacketGuildMenuRequest : public CPacket
{
public:
    CPacketGuildMenuRequest();
};

class CPacketQuestMenuRequest : public CPacket
{
public:
    CPacketQuestMenuRequest();
};

class CPacketEquipLastWeapon : public CPacket
{
public:
    CPacketEquipLastWeapon();
};

class CPacketVirtueRequest : public CPacket
{
public:
    CPacketVirtueRequest(int buttonID);
};

class CPacketInvokeVirtueRequest : public CPacket
{
public:
    CPacketInvokeVirtueRequest(uint8_t id);
};

class CPacketMegaClilocRequestOld : public CPacket
{
public:
    CPacketMegaClilocRequestOld(int serial);
};

class CPacketMegaClilocRequest : public CPacket
{
public:
    CPacketMegaClilocRequest(vector<uint32_t> &list);
};

class CPacketChangeStatLockStateRequest : public CPacket
{
public:
    CPacketChangeStatLockStateRequest(uint8_t stat, uint8_t state);
};

class CPacketBookHeaderChangeOld : public CPacket
{
public:
    CPacketBookHeaderChangeOld(class CGumpBook *gump);
};

class CPacketBookHeaderChange : public CPacket
{
public:
    CPacketBookHeaderChange(class CGumpBook *gump);
};

class CPacketBookPageData : public CPacket
{
public:
    CPacketBookPageData(class CGumpBook *gump, int page);
};

class CPacketBookPageDataRequest : public CPacket
{
public:
    CPacketBookPageDataRequest(int serial, int page);
};

class CPacketBuyRequest : public CPacket
{
public:
    CPacketBuyRequest(class CGumpShop *gump);
};

class CPacketSellRequest : public CPacket
{
public:
    CPacketSellRequest(class CGumpShop *gump);
};

class CPacketUseCombatAbility : public CPacket
{
public:
    CPacketUseCombatAbility(uint8_t index);
};

class CPacketTargetSelectedObject : public CPacket
{
public:
    CPacketTargetSelectedObject(int useObjectSerial, int targetObjectSerial);
};

class CPacketToggleGargoyleFlying : public CPacket
{
public:
    CPacketToggleGargoyleFlying();
};

class CPacketCustomHouseDataReq : public CPacket
{
public:
    CPacketCustomHouseDataReq(int serial);
};

class CPacketStunReq : public CPacket
{
public:
    CPacketStunReq();
};

class CPacketDisarmReq : public CPacket
{
public:
    CPacketDisarmReq();
};

class CPacketResend : public CPacket
{
public:
    CPacketResend();
};

class CPacketWalkRequest : public CPacket
{
public:
    CPacketWalkRequest(uint8_t direction, uint8_t sequence, int fastWalkKey);
};

class CPacketCustomHouseBackup : public CPacket
{
public:
    CPacketCustomHouseBackup();
};

class CPacketCustomHouseRestore : public CPacket
{
public:
    CPacketCustomHouseRestore();
};

class CPacketCustomHouseCommit : public CPacket
{
public:
    CPacketCustomHouseCommit();
};

class CPacketCustomHouseBuildingExit : public CPacket
{
public:
    CPacketCustomHouseBuildingExit();
};

class CPacketCustomHouseGoToFloor : public CPacket
{
public:
    CPacketCustomHouseGoToFloor(uint8_t floor);
};

class CPacketCustomHouseSync : public CPacket
{
public:
    CPacketCustomHouseSync();
};

class CPacketCustomHouseClear : public CPacket
{
public:
    CPacketCustomHouseClear();
};

class CPacketCustomHouseRevert : public CPacket
{
public:
    CPacketCustomHouseRevert();
};

class CPacketCustomHouseResponse : public CPacket
{
public:
    CPacketCustomHouseResponse();
};

class CPacketCustomHouseAddItem : public CPacket
{
public:
    CPacketCustomHouseAddItem(uint16_t graphic, int x, int y);
};

class CPacketCustomHouseDeleteItem : public CPacket
{
public:
    CPacketCustomHouseDeleteItem(uint16_t graphic, int x, int y, int z);
};

class CPacketCustomHouseAddRoof : public CPacket
{
public:
    CPacketCustomHouseAddRoof(uint16_t graphic, int x, int y, int z);
};

class CPacketCustomHouseDeleteRoof : public CPacket
{
public:
    CPacketCustomHouseDeleteRoof(uint16_t graphic, int x, int y, int z);
};

class CPacketCustomHouseAddStair : public CPacket
{
public:
    CPacketCustomHouseAddStair(uint16_t graphic, int x, int y);
};

class CPacketCrossVersion : public CPacket
{
public:
    CPacketCrossVersion(int version);
};
