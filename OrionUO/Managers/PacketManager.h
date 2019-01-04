// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CPacketManager;
typedef void (CPacketManager::*PACKET_FUNCTION)();

#define ORION_SAVE_ALL_PACKETS 1
#define ORION_SAVE_PACKET 1

#if ORION_SAVE_ALL_PACKETS == 1
#define ORION_IGNORE_PACKET 1
#else
#define ORION_IGNORE_PACKET 0
#endif

enum PACKET_DIRECTION
{
    DIR_SEND = 0, //!От клиента серверу
    DIR_RECV,     //!От сервера клиенту
    DIR_BOTH      //!В обе стороны
};

class CPacketInfo
{
public:
    bool save;
    const char *Name;
    int Size;
    PACKET_DIRECTION Direction;
    PACKET_FUNCTION Handler;
};

struct GumpCoords
{
    int X;
    int Y;
};

struct HTMLGumpDataInfo
{
    GumpCoords *sGumpCoords;
    int Width;
    int Height;
    int TextID;
    int HaveBackground;
    int HaveScrollbar;
    int Color;
    bool IsXMF;
};

#define HANDLER_PACKET(name) void Handle##name()

class CPacketManager : public Wisp::CPacketReader
{
public:
    void ConfigureClientVersion(uint32_t newClientVersion);
    string AutoLoginNames = "";
    uint32_t ConfigSerial = 0;

private:
    ProtectedSection m_Mutex;

    static CPacketInfo m_Packets[0x100];
    std::unordered_map<uint32_t, GumpCoords> m_GumpsCoordsCache;
    vector<uint32_t> m_MegaClilocRequests;
    deque<vector<uint8_t>> m_PluginData;

    bool AutoLoginNameExists(const string &name);
    void AddHTMLGumps(class CGump *gump, vector<HTMLGumpDataInfo> &list);

protected:
    virtual void OnPacket();
    virtual void OnReadFailed();

    HANDLER_PACKET(LoginError);
    HANDLER_PACKET(ServerList);
    HANDLER_PACKET(RelayServer);
    HANDLER_PACKET(CharacterList);
    HANDLER_PACKET(ResendCharacterList);
    HANDLER_PACKET(LoginComplete);
    HANDLER_PACKET(SetTime);
    HANDLER_PACKET(EnterWorld);
    HANDLER_PACKET(UpdateHitpoints);
    HANDLER_PACKET(UpdateMana);
    HANDLER_PACKET(UpdateStamina);
    HANDLER_PACKET(MobileAttributes);
    HANDLER_PACKET(NewHealthbarUpdate);
    HANDLER_PACKET(UpdatePlayer);
    HANDLER_PACKET(CharacterStatus);
    HANDLER_PACKET(UpdateItem);
    HANDLER_PACKET(UpdateItemSA);
    HANDLER_PACKET(UpdateObject);
    HANDLER_PACKET(EquipItem);
    HANDLER_PACKET(UpdateContainedItem);
    HANDLER_PACKET(UpdateContainedItems);
    HANDLER_PACKET(DenyMoveItem);
    HANDLER_PACKET(EndDraggingItem);
    HANDLER_PACKET(DropItemAccepted);
    HANDLER_PACKET(DeleteObject);
    HANDLER_PACKET(UpdateCharacter);
    HANDLER_PACKET(Warmode);
    HANDLER_PACKET(PauseControl);
    HANDLER_PACKET(OpenPaperdoll);
    HANDLER_PACKET(ClientVersion);
    HANDLER_PACKET(Ping);
    HANDLER_PACKET(SetWeather);
    HANDLER_PACKET(PersonalLightLevel);
    HANDLER_PACKET(LightLevel);
    HANDLER_PACKET(EnableLockedFeatures);
    HANDLER_PACKET(OpenContainer);
    HANDLER_PACKET(UpdateSkills);
    HANDLER_PACKET(ExtendedCommand);
    HANDLER_PACKET(DenyWalk);
    HANDLER_PACKET(ConfirmWalk);
    HANDLER_PACKET(OpenUrl);
    HANDLER_PACKET(Target);
    HANDLER_PACKET(Talk);
    HANDLER_PACKET(UnicodeTalk);
    HANDLER_PACKET(ClientTalk);
    HANDLER_PACKET(MultiPlacement);
    HANDLER_PACKET(GraphicEffect);
    HANDLER_PACKET(DeathScreen);
    HANDLER_PACKET(PlaySoundEffect);
    HANDLER_PACKET(PlayMusic);
    HANDLER_PACKET(DragAnimation);
    HANDLER_PACKET(CorpseEquipment);
    HANDLER_PACKET(ASCIIPrompt);
    HANDLER_PACKET(UnicodePrompt);
    HANDLER_PACKET(CharacterAnimation);
    HANDLER_PACKET(NewCharacterAnimation);
    HANDLER_PACKET(DisplayQuestArrow);
    HANDLER_PACKET(ClientViewRange);
    HANDLER_PACKET(KrriosClientSpecial);
    HANDLER_PACKET(AssistVersion);
    HANDLER_PACKET(CharacterListNotification);
    HANDLER_PACKET(ErrorCode);
    HANDLER_PACKET(AttackCharacter);
    HANDLER_PACKET(Season);
    HANDLER_PACKET(DisplayDeath);
    HANDLER_PACKET(OpenChat);
    HANDLER_PACKET(DisplayClilocString);
    HANDLER_PACKET(MegaCliloc);
    HANDLER_PACKET(Damage);
    HANDLER_PACKET(BuffDebuff);
    HANDLER_PACKET(SecureTrading);
    HANDLER_PACKET(TextEntryDialog);
    HANDLER_PACKET(OpenMenu);
    HANDLER_PACKET(OpenGump);
    HANDLER_PACKET(OpenCompressedGump);
    HANDLER_PACKET(DyeData);
    HANDLER_PACKET(DisplayMap);
    HANDLER_PACKET(MapData);
    HANDLER_PACKET(TipWindow);
    HANDLER_PACKET(CharacterProfile);
    HANDLER_PACKET(BulletinBoardData);
    HANDLER_PACKET(OpenBook);
    HANDLER_PACKET(OpenBookNew);
    HANDLER_PACKET(BookData);
    HANDLER_PACKET(BuyList);
    HANDLER_PACKET(SellList);
    HANDLER_PACKET(BuyReply);
    HANDLER_PACKET(Logout);
    HANDLER_PACKET(OPLInfo);
    HANDLER_PACKET(CustomHouse);
    HANDLER_PACKET(OrionMessages);
    HANDLER_PACKET(PacketsList);
    HANDLER_PACKET(MovePlayer);
    HANDLER_PACKET(Pathfinding);
    HANDLER_PACKET(BoatMoving);

    //Не обработаны
    /*
0x15 BMSG("Follow", 0x09),
0x2F RMSG("Combat Notification", 0x0a),
0xB2 BMSG("Chat Data", SIZE_VARIABLE),
0xB3 RMSG("Chat Text ?", SIZE_VARIABLE),
0xB5 BMSG("Open Chat Window", 0x40),
0xB7 RMSG("Popup Help Data", SIZE_VARIABLE),

0x15, 0x2F, 0x30, 0x31, 0x76, 0x7B, 0x81, 0x8B,
0x98, 0xB2, 0xB3, 0xB7, 0xBB, 0xC3, 0xC4, 0xC6, 0xC9,
0xCA, 0xCB, 0xCD, 0xCE, 0xD0, 0xDA, 0xDB, 0xDE, 0xE5, 0xE6,
0xE7, 0xE9, 0xEA, 0xF2, 0xF6
*/

public:
    CPacketManager();
    virtual ~CPacketManager();

    virtual int GetPacketSize(const vector<uint8_t> &packet, int &offsetToSize);
    CPacketInfo &GetInfo(uint8_t buf) const { return m_Packets[buf]; }
    void SendMegaClilocRequests();
    void AddMegaClilocRequest(int serial);
    void SavePluginReceivePacket(uint8_t *buf, int size);
    void ProcessPluginPackets();
    void PluginReceiveHandler(uint8_t *buf, int size);
    void SetCachedGumpCoords(uint32_t id, int x, int y);
};

extern CPacketManager g_PacketManager;
