// MIT License
// Copyright (C) August 2016 Hotride

#include "PacketManager.h"
#include "../Sockets.h"
#include "../Config.h"
#include <miniz.h>

CPacketManager g_PacketManager;

#define UMSG(save, size)                                                                           \
    {                                                                                              \
        save, "?", size, DIR_BOTH, 0                                                               \
    }
// A message type sent to the server
#define SMSG(save, name, size)                                                                     \
    {                                                                                              \
        save, name, size, DIR_SEND, 0                                                              \
    }
// A message type received from the server
#define RMSG(save, name, size)                                                                     \
    {                                                                                              \
        save, name, size, DIR_RECV, 0                                                              \
    }
// A message type transmitted in both directions
#define BMSG(save, name, size)                                                                     \
    {                                                                                              \
        save, name, size, DIR_BOTH, 0                                                              \
    }
// Message types that have handler methods
#define RMSGH(save, name, size, rmethod)                                                           \
    {                                                                                              \
        save, name, size, DIR_RECV, &CPacketManager::Handle##rmethod                               \
    }
#define BMSGH(save, name, size, rmethod)                                                           \
    {                                                                                              \
        save, name, size, DIR_BOTH, &CPacketManager::Handle##rmethod                               \
    }

CPacketInfo CPacketManager::m_Packets[0x100] = {
    /*0x00*/ SMSG(ORION_SAVE_PACKET, "Create Character", 0x68),
    /*0x01*/ SMSG(ORION_SAVE_PACKET, "Disconnect", 0x05),
    /*0x02*/ SMSG(ORION_IGNORE_PACKET, "Walk Request", 0x07),
    /*0x03*/ BMSGH(ORION_SAVE_PACKET, "Client Talk", PACKET_VARIABLE_SIZE, ClientTalk),
    /*0x04*/ BMSG(ORION_SAVE_PACKET, "Request God mode (God client)", 0x02),
    /*0x05*/ SMSG(ORION_IGNORE_PACKET, "Attack", 0x05),
    /*0x06*/ SMSG(ORION_IGNORE_PACKET, "Double Click", 0x05),
    /*0x07*/ SMSG(ORION_SAVE_PACKET, "Pick Up Item", 0x07),
    /*0x08*/ SMSG(ORION_SAVE_PACKET, "Drop Item", 0x0e),
    /*0x09*/ SMSG(ORION_IGNORE_PACKET, "Single Click", 0x05),
    /*0x0A*/ BMSG(ORION_SAVE_PACKET, "Edit (God client)", 0x0b),
    /*0x0B*/ RMSGH(ORION_IGNORE_PACKET, "Damage Visualization", 0x07, Damage),
    /*0x0C*/ BMSG(ORION_SAVE_PACKET, "Edit tiledata (God client)", PACKET_VARIABLE_SIZE),
    /*0x0D*/ BMSG(ORION_SAVE_PACKET, "Edit NPC data (God client)", 0x03),
    /*0x0E*/ BMSG(ORION_SAVE_PACKET, "Edit template data (God client)", 0x01),
    /*0x0F*/ UMSG(ORION_SAVE_PACKET, 0x3d),
    /*0x10*/ BMSG(ORION_SAVE_PACKET, "Edit hue data (God client)", 0xd7),
    /*0x11*/ RMSGH(ORION_IGNORE_PACKET, "Character Status", PACKET_VARIABLE_SIZE, CharacterStatus),
    /*0x12*/ SMSG(ORION_IGNORE_PACKET, "Perform Action", PACKET_VARIABLE_SIZE),
    /*0x13*/ SMSG(ORION_IGNORE_PACKET, "Client Equip Item", 0x0a),
    /*0x14*/ BMSG(ORION_SAVE_PACKET, "Change tile Z (God client)", 0x06),
    /*0x15*/ BMSG(ORION_SAVE_PACKET, "Follow", 0x09),
    /*0x16*/ RMSGH(ORION_SAVE_PACKET, "Health status bar update (0x16)", 0x01, NewHealthbarUpdate),
    /*0x17*/
    RMSGH(
        ORION_IGNORE_PACKET,
        "Health status bar update (KR)",
        PACKET_VARIABLE_SIZE,
        NewHealthbarUpdate),
    /*0x18*/ BMSG(ORION_SAVE_PACKET, "Add script (God client)", PACKET_VARIABLE_SIZE),
    /*0x19*/ BMSG(ORION_SAVE_PACKET, "Edit NPC speech (God client)", PACKET_VARIABLE_SIZE),
    /*0x1A*/ RMSGH(ORION_SAVE_PACKET, "Update Item", PACKET_VARIABLE_SIZE, UpdateItem),
    /*0x1B*/ RMSGH(ORION_SAVE_PACKET, "Enter World", 0x25, EnterWorld),
    /*0x1C*/ RMSGH(ORION_IGNORE_PACKET, "Server Talk", PACKET_VARIABLE_SIZE, Talk),
    /*0x1D*/ RMSGH(ORION_SAVE_PACKET, "Delete Object", 0x05, DeleteObject),
    /*0x1E*/ BMSG(ORION_SAVE_PACKET, "Animate?", 0x04),
    /*0x1F*/ BMSG(ORION_SAVE_PACKET, "Explode?", 0x08),
    /*0x20*/ RMSGH(ORION_SAVE_PACKET, "Update Player", 0x13, UpdatePlayer),
    /*0x21*/ RMSGH(ORION_IGNORE_PACKET, "Deny Walk", 0x08, DenyWalk),
    /*0x22*/ BMSGH(ORION_IGNORE_PACKET, "Confirm Walk", 0x03, ConfirmWalk),
    /*0x23*/ RMSGH(ORION_SAVE_PACKET, "Drag Animation", 0x1a, DragAnimation),
    /*0x24*/ RMSGH(ORION_SAVE_PACKET, "Open Container", 0x07, OpenContainer),
    /*0x25*/ RMSGH(ORION_SAVE_PACKET, "Update Contained Item", 0x14, UpdateContainedItem),
    /*0x26*/ BMSG(ORION_SAVE_PACKET, "Kick client (God client)", 0x05),
    /*0x27*/ RMSGH(ORION_SAVE_PACKET, "Deny Move Item", 0x02, DenyMoveItem),
    /*0x28*/ RMSGH(ORION_SAVE_PACKET, "End dragging item", 0x05, EndDraggingItem),
    /*0x29*/ RMSGH(ORION_SAVE_PACKET, "Drop Item Accepted", 0x01, DropItemAccepted),
    /*0x2A*/ RMSG(ORION_SAVE_PACKET, "Blood mode", 0x05),
    /*0x2B*/ BMSG(ORION_SAVE_PACKET, "Toggle God mode (God client)", 0x02),
    /*0x2C*/ BMSGH(ORION_IGNORE_PACKET, "Death Screen", 0x02, DeathScreen),
    /*0x2D*/ RMSGH(ORION_SAVE_PACKET, "Mobile Attributes", 0x11, MobileAttributes),
    /*0x2E*/ RMSGH(ORION_SAVE_PACKET, "Server Equip Item", 0x0f, EquipItem),
    /*0x2F*/ RMSG(ORION_SAVE_PACKET, "Combat Notification", 0x0a),
    /*0x30*/ RMSG(ORION_SAVE_PACKET, "Attack ok", 0x05),
    /*0x31*/ RMSG(ORION_SAVE_PACKET, "Attack end", 0x01),
    /*0x32*/ BMSG(ORION_SAVE_PACKET, "Toggle hack mover (God client)", 0x02),
    /*0x33*/ RMSGH(ORION_IGNORE_PACKET, "Pause Control", 0x02, PauseControl),
    /*0x34*/ SMSG(ORION_IGNORE_PACKET, "Status Request", 0x0a),
    /*0x35*/ BMSG(ORION_SAVE_PACKET, "Resource type (God client)", 0x28d),
    /*0x36*/ BMSG(ORION_SAVE_PACKET, "Resource tile data (God client)", PACKET_VARIABLE_SIZE),
    /*0x37*/ BMSG(ORION_SAVE_PACKET, "Move object (God client)", 0x08),
    /*0x38*/ RMSGH(ORION_SAVE_PACKET, "Pathfinding", 0x07, Pathfinding),
    /*0x39*/ BMSG(ORION_SAVE_PACKET, "Remove group (God client)", 0x09),
    /*0x3A*/ BMSGH(ORION_IGNORE_PACKET, "Update Skills", PACKET_VARIABLE_SIZE, UpdateSkills),
    /*0x3B*/ BMSGH(ORION_IGNORE_PACKET, "Vendor Buy Reply", PACKET_VARIABLE_SIZE, BuyReply),
    /*0x3C*/
    RMSGH(ORION_SAVE_PACKET, "Update Contained Items", PACKET_VARIABLE_SIZE, UpdateContainedItems),
    /*0x3D*/ BMSG(ORION_SAVE_PACKET, "Ship (God client)", 0x02),
    /*0x3E*/ BMSG(ORION_SAVE_PACKET, "Versions (God client)", 0x25),
    /*0x3F*/ BMSG(ORION_SAVE_PACKET, "Update Statics (God Client)", PACKET_VARIABLE_SIZE),
    /*0x40*/ BMSG(ORION_SAVE_PACKET, "Update terrains (God client)", 0xc9),
    /*0x41*/ BMSG(ORION_SAVE_PACKET, "Update terrains (God client)", PACKET_VARIABLE_SIZE),
    /*0x42*/ BMSG(ORION_SAVE_PACKET, "Update art (God client)", PACKET_VARIABLE_SIZE),
    /*0x43*/ BMSG(ORION_SAVE_PACKET, "Update animation (God client)", 0x229),
    /*0x44*/ BMSG(ORION_SAVE_PACKET, "Update hues (God client)", 0x2c9),
    /*0x45*/ BMSG(ORION_SAVE_PACKET, "Version OK (God client)", 0x05),
    /*0x46*/ BMSG(ORION_SAVE_PACKET, "New art (God client)", PACKET_VARIABLE_SIZE),
    /*0x47*/ BMSG(ORION_SAVE_PACKET, "New terrain (God client)", 0x0b),
    /*0x48*/ BMSG(ORION_SAVE_PACKET, "New animation (God client)", 0x49),
    /*0x49*/ BMSG(ORION_SAVE_PACKET, "New hues (God client)", 0x5d),
    /*0x4A*/ BMSG(ORION_SAVE_PACKET, "Destroy art (God client)", 0x05),
    /*0x4B*/ BMSG(ORION_SAVE_PACKET, "Check version (God client)", 0x09),
    /*0x4C*/ BMSG(ORION_SAVE_PACKET, "Script names (God client)", PACKET_VARIABLE_SIZE),
    /*0x4D*/ BMSG(ORION_SAVE_PACKET, "Edit script (God client)", PACKET_VARIABLE_SIZE),
    /*0x4E*/ RMSGH(ORION_IGNORE_PACKET, "Personal Light Level", 0x06, PersonalLightLevel),
    /*0x4F*/ RMSGH(ORION_IGNORE_PACKET, "Global Light Level", 0x02, LightLevel),
    /*0x50*/ BMSG(ORION_IGNORE_PACKET, "Board header", PACKET_VARIABLE_SIZE),
    /*0x51*/ BMSG(ORION_IGNORE_PACKET, "Board message", PACKET_VARIABLE_SIZE),
    /*0x52*/ BMSG(ORION_IGNORE_PACKET, "Post board message", PACKET_VARIABLE_SIZE),
    /*0x53*/ RMSGH(ORION_SAVE_PACKET, "Error Code", 0x02, ErrorCode),
    /*0x54*/ RMSGH(ORION_SAVE_PACKET, "Sound Effect", 0x0c, PlaySoundEffect),
    /*0x55*/ RMSGH(ORION_IGNORE_PACKET, "Login Complete", 0x01, LoginComplete),
    /*0x56*/ BMSGH(ORION_IGNORE_PACKET, "Map Data", 0x0b, MapData),
    /*0x57*/ BMSG(ORION_SAVE_PACKET, "Update regions (God client)", 0x6e),
    /*0x58*/ BMSG(ORION_SAVE_PACKET, "New region (God client)", 0x6a),
    /*0x59*/ BMSG(ORION_SAVE_PACKET, "New content FX (God client)", PACKET_VARIABLE_SIZE),
    /*0x5A*/ BMSG(ORION_SAVE_PACKET, "Update content FX (God client)", PACKET_VARIABLE_SIZE),
    /*0x5B*/ RMSGH(ORION_IGNORE_PACKET, "Set Time", 0x04, SetTime),
    /*0x5C*/ BMSG(ORION_SAVE_PACKET, "Restart Version", 0x02),
    /*0x5D*/ SMSG(ORION_IGNORE_PACKET, "Select Character", 0x49),
    /*0x5E*/ BMSG(ORION_SAVE_PACKET, "Server list (God client)", PACKET_VARIABLE_SIZE),
    /*0x5F*/ BMSG(ORION_SAVE_PACKET, "Add server (God client)", 0x31),
    /*0x60*/ BMSG(ORION_SAVE_PACKET, "Remove server (God client)", 0x05),
    /*0x61*/ BMSG(ORION_SAVE_PACKET, "Destroy static (God client)", 0x09),
    /*0x62*/ BMSG(ORION_SAVE_PACKET, "Move static (God client)", 0x0f),
    /*0x63*/ BMSG(ORION_SAVE_PACKET, "Area load (God client)", 0x0d),
    /*0x64*/ BMSG(ORION_SAVE_PACKET, "Area load request (God client)", 0x01),
    /*0x65*/ RMSGH(ORION_IGNORE_PACKET, "Set Weather", 0x04, SetWeather),
    /*0x66*/ BMSGH(ORION_IGNORE_PACKET, "Book Page Data", PACKET_VARIABLE_SIZE, BookData),
    /*0x67*/ BMSG(ORION_SAVE_PACKET, "Simped? (God client)", 0x15),
    /*0x68*/ BMSG(ORION_SAVE_PACKET, "Script attach (God client)", PACKET_VARIABLE_SIZE),
    /*0x69*/ BMSG(ORION_SAVE_PACKET, "Friends (God client)", PACKET_VARIABLE_SIZE),
    /*0x6A*/ BMSG(ORION_SAVE_PACKET, "Notify friend (God client)", 0x03),
    /*0x6B*/ BMSG(ORION_SAVE_PACKET, "Key use (God client)", 0x09),
    /*0x6C*/ BMSGH(ORION_IGNORE_PACKET, "Target Data", 0x13, Target),
    /*0x6D*/ RMSGH(ORION_SAVE_PACKET, "Play Music", 0x03, PlayMusic),
    /*0x6E*/ RMSGH(ORION_IGNORE_PACKET, "Character Animation", 0x0e, CharacterAnimation),
    /*0x6F*/ BMSGH(ORION_IGNORE_PACKET, "Secure Trading", PACKET_VARIABLE_SIZE, SecureTrading),
    /*0x70*/ RMSGH(ORION_IGNORE_PACKET, "Graphic Effect", 0x1c, GraphicEffect),
    /*0x71*/
    BMSGH(ORION_IGNORE_PACKET, "Bulletin Board Data", PACKET_VARIABLE_SIZE, BulletinBoardData),
    /*0x72*/ BMSGH(ORION_IGNORE_PACKET, "War Mode", 0x05, Warmode),
    /*0x73*/ BMSGH(ORION_IGNORE_PACKET, "Ping", 0x02, Ping),
    /*0x74*/ RMSGH(ORION_IGNORE_PACKET, "Vendor Buy List", PACKET_VARIABLE_SIZE, BuyList),
    /*0x75*/ SMSG(ORION_SAVE_PACKET, "Rename Character", 0x23),
    /*0x76*/ RMSG(ORION_SAVE_PACKET, "New Subserver", 0x10),
    /*0x77*/ RMSGH(ORION_SAVE_PACKET, "Update Character", 0x11, UpdateCharacter),
    /*0x78*/ RMSGH(ORION_SAVE_PACKET, "Update Object", PACKET_VARIABLE_SIZE, UpdateObject),
    /*0x79*/ BMSG(ORION_SAVE_PACKET, "Resource query (God client)", 0x09),
    /*0x7A*/ BMSG(ORION_SAVE_PACKET, "Resource data (God client)", PACKET_VARIABLE_SIZE),
    /*0x7B*/ RMSG(ORION_SAVE_PACKET, "Sequence?", 0x02),
    /*0x7C*/ RMSGH(ORION_IGNORE_PACKET, "Open Menu Gump", PACKET_VARIABLE_SIZE, OpenMenu),
    /*0x7D*/ SMSG(ORION_IGNORE_PACKET, "Menu Choice", 0x0d),
    /*0x7E*/ BMSG(ORION_SAVE_PACKET, "God view query (God client)", 0x02),
    /*0x7F*/ BMSG(ORION_SAVE_PACKET, "God view data (God client)", PACKET_VARIABLE_SIZE),
    /*0x80*/ SMSG(ORION_IGNORE_PACKET, "First Login", 0x3e),
    /*0x81*/ RMSG(ORION_SAVE_PACKET, "Change character", PACKET_VARIABLE_SIZE),
    /*0x82*/ RMSGH(ORION_IGNORE_PACKET, "Login Error", 0x02, LoginError),
    /*0x83*/ SMSG(ORION_IGNORE_PACKET, "Delete Character", 0x27),
    /*0x84*/ BMSG(ORION_SAVE_PACKET, "Change password", 0x45),
    /*0x85*/
    RMSGH(ORION_IGNORE_PACKET, "Character List Notification", 0x02, CharacterListNotification),
    /*0x86*/
    RMSGH(ORION_IGNORE_PACKET, "Resend Character List", PACKET_VARIABLE_SIZE, ResendCharacterList),
    /*0x87*/ BMSG(ORION_SAVE_PACKET, "Send resources (God client)", PACKET_VARIABLE_SIZE),
    /*0x88*/ RMSGH(ORION_IGNORE_PACKET, "Open Paperdoll", 0x42, OpenPaperdoll),
    /*0x89*/ RMSGH(ORION_SAVE_PACKET, "Corpse Equipment", PACKET_VARIABLE_SIZE, CorpseEquipment),
    /*0x8A*/ BMSG(ORION_SAVE_PACKET, "Trigger edit (God client)", PACKET_VARIABLE_SIZE),
    /*0x8B*/ RMSG(ORION_SAVE_PACKET, "Display sign gump", PACKET_VARIABLE_SIZE),
    /*0x8C*/ RMSGH(ORION_IGNORE_PACKET, "Relay Server", 0x0b, RelayServer),
    /*0x8D*/ SMSG(ORION_SAVE_PACKET, "UO3D create character", PACKET_VARIABLE_SIZE),
    /*0x8E*/ BMSG(ORION_SAVE_PACKET, "Move character (God client)", PACKET_VARIABLE_SIZE),
    /*0x8F*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0x90*/ RMSGH(ORION_IGNORE_PACKET, "Display Map", 0x13, DisplayMap),
    /*0x91*/ SMSG(ORION_IGNORE_PACKET, "Second Login", 0x41),
    /*0x92*/ BMSG(ORION_SAVE_PACKET, "Update multi data (God client)", PACKET_VARIABLE_SIZE),
    /*0x93*/ RMSGH(ORION_IGNORE_PACKET, "Open Book", 0x63, OpenBook),
    /*0x94*/ BMSG(ORION_SAVE_PACKET, "Update skills data (God client)", PACKET_VARIABLE_SIZE),
    /*0x95*/ BMSGH(ORION_IGNORE_PACKET, "Dye Data", 0x09, DyeData),
    /*0x96*/ BMSG(ORION_SAVE_PACKET, "Game central monitor (God client)", PACKET_VARIABLE_SIZE),
    /*0x97*/ RMSGH(ORION_SAVE_PACKET, "Move Player", 0x02, MovePlayer),
    /*0x98*/ BMSG(ORION_SAVE_PACKET, "All Names (3D Client Only)", PACKET_VARIABLE_SIZE),
    /*0x99*/ BMSGH(ORION_SAVE_PACKET, "Multi Placement", 0x1a, MultiPlacement),
    /*0x9A*/ BMSGH(ORION_SAVE_PACKET, "ASCII Prompt", PACKET_VARIABLE_SIZE, ASCIIPrompt),
    /*0x9B*/ SMSG(ORION_IGNORE_PACKET, "Help Request", 0x102),
    /*0x9C*/ BMSG(ORION_SAVE_PACKET, "Assistant request (God client)", 0x135),
    /*0x9D*/ BMSG(ORION_SAVE_PACKET, "GM single (God client)", 0x33),
    /*0x9E*/ RMSGH(ORION_IGNORE_PACKET, "Vendor Sell List", PACKET_VARIABLE_SIZE, SellList),
    /*0x9F*/ SMSG(ORION_IGNORE_PACKET, "Vendor Sell Reply", PACKET_VARIABLE_SIZE),
    /*0xA0*/ SMSG(ORION_IGNORE_PACKET, "Select Server", 0x03),
    /*0xA1*/ RMSGH(ORION_IGNORE_PACKET, "Update Hitpoints", 0x09, UpdateHitpoints),
    /*0xA2*/ RMSGH(ORION_IGNORE_PACKET, "Update Mana", 0x09, UpdateMana),
    /*0xA3*/ RMSGH(ORION_IGNORE_PACKET, "Update Stamina", 0x09, UpdateStamina),
    /*0xA4*/ SMSG(ORION_SAVE_PACKET, "System Information", 0x95),
    /*0xA5*/ RMSGH(ORION_SAVE_PACKET, "Open URL", PACKET_VARIABLE_SIZE, OpenUrl),
    /*0xA6*/ RMSGH(ORION_IGNORE_PACKET, "Tip Window", PACKET_VARIABLE_SIZE, TipWindow),
    /*0xA7*/ SMSG(ORION_SAVE_PACKET, "Request Tip", 0x04),
    /*0xA8*/ RMSGH(ORION_IGNORE_PACKET, "Server List", PACKET_VARIABLE_SIZE, ServerList),
    /*0xA9*/ RMSGH(ORION_IGNORE_PACKET, "Character List", PACKET_VARIABLE_SIZE, CharacterList),
    /*0xAA*/ RMSGH(ORION_IGNORE_PACKET, "Attack Reply", 0x05, AttackCharacter),
    /*0xAB*/ RMSGH(ORION_SAVE_PACKET, "Text Entry Dialog", PACKET_VARIABLE_SIZE, TextEntryDialog),
    /*0xAC*/ SMSG(ORION_SAVE_PACKET, "Text Entry Dialog Reply", PACKET_VARIABLE_SIZE),
    /*0xAD*/ SMSG(ORION_IGNORE_PACKET, "Unicode Client Talk", PACKET_VARIABLE_SIZE),
    /*0xAE*/ RMSGH(ORION_IGNORE_PACKET, "Unicode Server Talk", PACKET_VARIABLE_SIZE, UnicodeTalk),
    /*0xAF*/ RMSGH(ORION_SAVE_PACKET, "Display Death", 0x0d, DisplayDeath),
    /*0xB0*/ RMSGH(ORION_IGNORE_PACKET, "Open Gump", PACKET_VARIABLE_SIZE, OpenGump),
    /*0xB1*/ SMSG(ORION_IGNORE_PACKET, "Gump Choice", PACKET_VARIABLE_SIZE),
    /*0xB2*/ BMSG(ORION_SAVE_PACKET, "Chat Data", PACKET_VARIABLE_SIZE),
    /*0xB3*/ RMSG(ORION_SAVE_PACKET, "Chat Text ?", PACKET_VARIABLE_SIZE),
    /*0xB4*/ BMSG(ORION_SAVE_PACKET, "Target object list (God client)", PACKET_VARIABLE_SIZE),
    /*0xB5*/ BMSGH(ORION_SAVE_PACKET, "Open Chat Window", 0x40, OpenChat),
    /*0xB6*/ SMSG(ORION_SAVE_PACKET, "Popup Help Request", 0x09),
    /*0xB7*/ RMSG(ORION_SAVE_PACKET, "Popup Help Data", PACKET_VARIABLE_SIZE),
    /*0xB8*/
    BMSGH(ORION_IGNORE_PACKET, "Character Profile", PACKET_VARIABLE_SIZE, CharacterProfile),
    /*0xB9*/ RMSGH(ORION_SAVE_PACKET, "Enable locked client features", 0x03, EnableLockedFeatures),
    /*0xBA*/ RMSGH(ORION_IGNORE_PACKET, "Display Quest Arrow", 0x06, DisplayQuestArrow),
    /*0xBB*/ SMSG(ORION_SAVE_PACKET, "Account ID ?", 0x09),
    /*0xBC*/ RMSGH(ORION_IGNORE_PACKET, "Season", 0x03, Season),
    /*0xBD*/ BMSGH(ORION_SAVE_PACKET, "Client Version", PACKET_VARIABLE_SIZE, ClientVersion),
    /*0xBE*/ BMSGH(ORION_SAVE_PACKET, "Assist Version", PACKET_VARIABLE_SIZE, AssistVersion),
    /*0xBF*/ BMSGH(ORION_SAVE_PACKET, "Extended Command", PACKET_VARIABLE_SIZE, ExtendedCommand),
    /*0xC0*/ RMSGH(ORION_IGNORE_PACKET, "Graphical Effect", 0x24, GraphicEffect),
    /*0xC1*/
    RMSGH(ORION_IGNORE_PACKET, "Display cliloc String", PACKET_VARIABLE_SIZE, DisplayClilocString),
    /*0xC2*/ BMSGH(ORION_SAVE_PACKET, "Unicode prompt", PACKET_VARIABLE_SIZE, UnicodePrompt),
    /*0xC3*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xC4*/ UMSG(ORION_SAVE_PACKET, 0x06),
    /*0xC5*/ BMSG(ORION_SAVE_PACKET, "Invalid map (God client)", 0xcb),
    /*0xC6*/ RMSG(ORION_SAVE_PACKET, "Invalid map enable", 0x01),
    /*0xC7*/ RMSGH(ORION_IGNORE_PACKET, "Graphical Effect", 0x31, GraphicEffect),
    /*0xC8*/ BMSGH(ORION_SAVE_PACKET, "Client View Range", 0x02, ClientViewRange),
    /*0xC9*/ BMSG(ORION_SAVE_PACKET, "Trip time", 0x06),
    /*0xCA*/ BMSG(ORION_SAVE_PACKET, "UTrip time", 0x06),
    /*0xCB*/ UMSG(ORION_SAVE_PACKET, 0x07),
    /*0xCC*/
    RMSGH(
        ORION_IGNORE_PACKET,
        "Localized Text Plus String",
        PACKET_VARIABLE_SIZE,
        DisplayClilocString),
    /*0xCD*/ UMSG(ORION_SAVE_PACKET, 0x01),
    /*0xCE*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xCF*/ UMSG(ORION_SAVE_PACKET, 0x4e),
    /*0xD0*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xD1*/ RMSGH(ORION_IGNORE_PACKET, "Logout", 0x02, Logout),
    /*0xD2*/ RMSGH(ORION_SAVE_PACKET, "Update Character (New)", 0x19, UpdateCharacter),
    /*0xD3*/ RMSGH(ORION_SAVE_PACKET, "Update Object (New)", PACKET_VARIABLE_SIZE, UpdateObject),
    /*0xD4*/ BMSGH(ORION_IGNORE_PACKET, "Open Book (New)", PACKET_VARIABLE_SIZE, OpenBookNew),
    /*0xD5*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xD6*/ BMSGH(ORION_IGNORE_PACKET, "Mega cliloc", PACKET_VARIABLE_SIZE, MegaCliloc),
    /*0xD7*/ SMSG(ORION_SAVE_PACKET, "+AoS command", PACKET_VARIABLE_SIZE),
    /*0xD8*/ RMSGH(ORION_IGNORE_PACKET, "Custom house", PACKET_VARIABLE_SIZE, CustomHouse),
    /*0xD9*/ SMSG(ORION_SAVE_PACKET, "+Metrics", 0x10c),
    /*0xDA*/ BMSG(ORION_SAVE_PACKET, "Mahjong game command", PACKET_VARIABLE_SIZE),
    /*0xDB*/ RMSG(ORION_SAVE_PACKET, "Character transfer log", PACKET_VARIABLE_SIZE),
    /*0xDC*/ RMSGH(ORION_IGNORE_PACKET, "OPL Info Packet", 9, OPLInfo),
    /*0xDD*/
    RMSGH(ORION_IGNORE_PACKET, "Compressed Gump", PACKET_VARIABLE_SIZE, OpenCompressedGump),
    /*0xDE*/ RMSG(ORION_SAVE_PACKET, "Update characters combatants", PACKET_VARIABLE_SIZE),
    /*0xDF*/ RMSGH(ORION_SAVE_PACKET, "Buff/Debuff", PACKET_VARIABLE_SIZE, BuffDebuff),
    /*0xE0*/ SMSG(ORION_SAVE_PACKET, "Bug Report KR", PACKET_VARIABLE_SIZE),
    /*0xE1*/ SMSG(ORION_SAVE_PACKET, "Client Type KR/SA", 0x09),
    /*0xE2*/ RMSGH(ORION_IGNORE_PACKET, "New Character Animation", 0xa, NewCharacterAnimation),
    /*0xE3*/ RMSG(ORION_SAVE_PACKET, "KR Encryption Responce", 0x4d),
    /*0xE4*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xE5*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xE6*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xE7*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xE8*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xE9*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xEA*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xEB*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xEC*/ SMSG(ORION_SAVE_PACKET, "Equip Macro", PACKET_VARIABLE_SIZE),
    /*0xED*/ SMSG(ORION_SAVE_PACKET, "Unequip item macro", PACKET_VARIABLE_SIZE),
    /*0xEE*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xEF*/ SMSG(ORION_SAVE_PACKET, "KR/2D Client Login/Seed", 0x15),
    /*0xF0*/
    BMSGH(ORION_SAVE_PACKET, "Krrios client special", PACKET_VARIABLE_SIZE, KrriosClientSpecial),
    /*0xF1*/
    SMSG(ORION_SAVE_PACKET, "Client-Server Time Synchronization Request", PACKET_VARIABLE_SIZE),
    /*0xF2*/
    RMSG(ORION_SAVE_PACKET, "Client-Server Time Synchronization Response", PACKET_VARIABLE_SIZE),
    /*0xF3*/ RMSGH(ORION_SAVE_PACKET, "Update Item (SA)", 0x18, UpdateItemSA),
    /*0xF4*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xF5*/ RMSGH(ORION_IGNORE_PACKET, "Display New Map", 0x15, DisplayMap),
    /*0xF6*/ RMSGH(ORION_SAVE_PACKET, "Boat moving", PACKET_VARIABLE_SIZE, BoatMoving),
    /*0xF7*/ RMSGH(ORION_SAVE_PACKET, "Packets (0xF3) list", PACKET_VARIABLE_SIZE, PacketsList),
    /*0xF8*/ SMSG(ORION_SAVE_PACKET, "Character Creation (7.0.16.0)", 0x6a),
    /*0xF9*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xFA*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xFB*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xFC*/ BMSGH(ORION_SAVE_PACKET, "Orion messages", PACKET_VARIABLE_SIZE, OrionMessages),
    /*0xFD*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE),
    /*0xFE*/ RMSG(ORION_SAVE_PACKET, "Razor Handshake", 0x8),
    /*0xFF*/ UMSG(ORION_SAVE_PACKET, PACKET_VARIABLE_SIZE)
};

CPacketManager::CPacketManager()

{
    CREATE_MUTEX(m_Mutex);
}

CPacketManager::~CPacketManager()
{
    RELEASE_MUTEX(m_Mutex);
}

bool CPacketManager::AutoLoginNameExists(const string &name)
{
    DEBUG_TRACE_FUNCTION;

    if (AutoLoginNames.length() == 0u)
    {
        return false;
    }

    string search = string("|") + name + "|";
    return (AutoLoginNames.find(search) != string::npos);
}

#define CV_PRINT 0

#if CV_PRINT != 0
#define CVPRINT(s) LOG(s)
#else //CV_PRINT==0
#define CVPRINT(s)
#endif //CV_PRINT!=0

void CPacketManager::ConfigureClientVersion(uint32_t newClientVersion)
{
    DEBUG_TRACE_FUNCTION;

    if (newClientVersion >= CV_500A)
    {
        CVPRINT("Set new length for packet 0x0B (>= 5.0.0a)\n");
        m_Packets[0x0B].Size = 0x07;
        CVPRINT("Set new length for packet 0x16 (>= 5.0.0a)\n");
        m_Packets[0x16].Size = PACKET_VARIABLE_SIZE;
        CVPRINT("Set new length for packet 0x31 (>= 5.0.0a)\n");
        m_Packets[0x31].Size = PACKET_VARIABLE_SIZE;
    }
    else
    {
        CVPRINT("Set standart length for packet 0x0B (< 5.0.0a)\n");
        m_Packets[0x0B].Size = 0x10A;
        CVPRINT("Set standart length for packet 0x16 (< 5.0.0a)\n");
        m_Packets[0x16].Size = 0x01;
        CVPRINT("Set standart length for packet 0x31 (< 5.0.0a)\n");
        m_Packets[0x31].Size = 0x01;
    }

    if (newClientVersion >= CV_5090)
    {
        CVPRINT("Set new length for packet 0xE1 (>= 5.0.9.0)\n");
        m_Packets[0xE1].Size = PACKET_VARIABLE_SIZE;
    }
    else
    {
        CVPRINT("Set standart length for packet 0xE1 (<= 5.0.9.0)\n");
        m_Packets[0xE1].Size = 0x09;
    }

    if (newClientVersion >= CV_6013)
    {
        CVPRINT("Set new length for packet 0xE3 (>= 6.0.1.3)\n");
        m_Packets[0xE3].Size = PACKET_VARIABLE_SIZE;
        CVPRINT("Set new length for packet 0xE6 (>= 6.0.1.3)\n");
        m_Packets[0xE6].Size = 0x05;
        CVPRINT("Set new length for packet 0xE7 (>= 6.0.1.3)\n");
        m_Packets[0xE7].Size = 0x0C;
        CVPRINT("Set new length for packet 0xE8 (>= 6.0.1.3)\n");
        m_Packets[0xE8].Size = 0x0D;
        CVPRINT("Set new length for packet 0xE9 (>= 6.0.1.3)\n");
        m_Packets[0xE9].Size = 0x4B;
        CVPRINT("Set new length for packet 0xEA (>= 6.0.1.3)\n");
        m_Packets[0xEA].Size = 0x03;
    }
    else
    {
        CVPRINT("Set standart length for packet 0xE3 (<= 6.0.1.3)\n");
        m_Packets[0xE3].Size = 0x4D;
        CVPRINT("Set standart length for packet 0xE6 (<= 6.0.1.3)\n");
        m_Packets[0xE6].Size = PACKET_VARIABLE_SIZE;
        CVPRINT("Set standart length for packet 0xE7 (<= 6.0.1.3)\n");
        m_Packets[0xE7].Size = PACKET_VARIABLE_SIZE;
        CVPRINT("Set standart length for packet 0xE8 (<= 6.0.1.3)\n");
        m_Packets[0xE8].Size = PACKET_VARIABLE_SIZE;
        CVPRINT("Set standart length for packet 0xE9 (<= 6.0.1.3)\n");
        m_Packets[0xE9].Size = PACKET_VARIABLE_SIZE;
        CVPRINT("Set standart length for packet 0xEA (<= 6.0.1.3)\n");
        m_Packets[0xEA].Size = PACKET_VARIABLE_SIZE;
    }

    if (newClientVersion >= CV_6017)
    {
        CVPRINT("Set new length for packet 0x08 (>= 6.0.1.7)\n");
        m_Packets[0x08].Size = 0x0F;
        CVPRINT("Set new length for packet 0x25 (>= 6.0.1.7)\n");
        m_Packets[0x25].Size = 0x15;
    }
    else
    {
        CVPRINT("Set standart length for packet 0x08 (<= 6.0.1.7)\n");
        m_Packets[0x08].Size = 0x0E;
        CVPRINT("Set standart length for packet 0x25 (<= 6.0.1.7)\n");
        m_Packets[0x25].Size = 0x14;
    }

    if (newClientVersion == CV_6060)
    {
        CVPRINT("Set new length for packet 0xEE (>= 6.0.6.0)\n");
        m_Packets[0xEE].Size = 0x2000;
        CVPRINT("Set new length for packet 0xEF (>= 6.0.6.0)\n");
        m_Packets[0xEF].Size = 0x2000;
        CVPRINT("Set new length for packet 0xF1 (>= 6.0.6.0)\n");
        m_Packets[0xF1].Size = 0x09;
    }
    else
    {
        CVPRINT("Set standart length for packet 0xEE (<= 6.0.6.0)\n");
        m_Packets[0xEE].Size = PACKET_VARIABLE_SIZE;
        CVPRINT("Set standart length for packet 0xEF (<= 6.0.6.0)\n");
        m_Packets[0xEF].Size = 0x15;
        CVPRINT("Set standart length for packet 0xF1 (<= 6.0.6.0)\n");
        m_Packets[0xF1].Size = PACKET_VARIABLE_SIZE;
    }

    if (newClientVersion >= CV_60142)
    {
        CVPRINT("Set new length for packet 0xB9 (>= 6.0.14.2)\n");
        m_Packets[0xB9].Size = 0x05;
    }
    else
    {
        CVPRINT("Set standart length for packet 0xB9 (<= 6.0.14.2)\n");
        m_Packets[0xB9].Size = 0x03;
    }

    if (newClientVersion >= CV_7000)
    {
        CVPRINT("Set new length for packet 0xEE (>= 7.0.0.0)\n");
        m_Packets[0xEE].Size = 0x2000;
        CVPRINT("Set new length for packet 0xEF (>= 7.0.0.0)\n");
        m_Packets[0xEF].Size = 0x2000;
        /*CVPRINT("Set new length for packet 0xF0 (>= 7.0.0.0)\n");
        m_Packets[0xF0].size = 0x2000;
        CVPRINT("Set new length for packet 0xF1 (>= 7.0.0.0)\n");
        m_Packets[0xF1].size = 0x2000;
        CVPRINT("Set new length for packet 0xF2 (>= 7.0.0.0)\n");
        m_Packets[0xF2].size = 0x2000;*/
    }
    else
    {
        CVPRINT("Set standart length for packet 0xEE (<= 7.0.0.0)\n");
        m_Packets[0xEE].Size = PACKET_VARIABLE_SIZE;
        CVPRINT("Set standart length for packet 0xEF (<= 7.0.0.0)\n");
        m_Packets[0xEF].Size = 0x15;
        /*CVPRINT("Set standart length for packet 0xF0 (<= 7.0.0.0)\n");
        m_Packets[0xF0].size = PACKET_VARIABLE_SIZE;
        CVPRINT("Set standart length for packet 0xF1 (<= 7.0.0.0)\n");
        m_Packets[0xF1].size = PACKET_VARIABLE_SIZE;
        CVPRINT("Set standart length for packet 0xF2 (<= 7.0.0.0)\n");
        m_Packets[0xF2].size = PACKET_VARIABLE_SIZE;*/
    }

    if (newClientVersion >= CV_7090)
    {
        CVPRINT("Set new length for packet 0x24 (>= 7.0.9.0)\n");
        m_Packets[0x24].Size = 0x09;
        CVPRINT("Set new length for packet 0x99 (>= 7.0.9.0)\n");
        m_Packets[0x99].Size = 0x1E;
        CVPRINT("Set new length for packet 0xBA (>= 7.0.9.0)\n");
        m_Packets[0xBA].Size = 0x0A;
        CVPRINT("Set new length for packet 0xF3 (>= 7.0.9.0)\n");
        m_Packets[0xF3].Size = 0x1A;

        // Already changed in client 7.0.8.2
        CVPRINT("Set new length for packet 0xF1 (>= 7.0.9.0)\n");
        m_Packets[0xF1].Size = 0x09;
        CVPRINT("Set new length for packet 0xF2 (>= 7.0.9.0)\n");
        m_Packets[0xF2].Size = 0x19;
    }
    else
    {
        CVPRINT("Set standart length for packet 0x24 (<= 7.0.9.0)\n");
        m_Packets[0x24].Size = 0x07;
        CVPRINT("Set standart length for packet 0x99 (<= 7.0.9.0)\n");
        m_Packets[0x99].Size = 0x1A;
        CVPRINT("Set standart length for packet 0xBA (<= 7.0.9.0)\n");
        m_Packets[0xBA].Size = 0x06;
        CVPRINT("Set standart length for packet 0xF3 (<= 7.0.9.0)\n");
        m_Packets[0xF3].Size = 0x18;

        // Already changed in client 7.0.8.2
        CVPRINT("Set standart length for packet 0xF1 (<= 7.0.9.0)\n");
        m_Packets[0xF1].Size = PACKET_VARIABLE_SIZE;
        CVPRINT("Set standart length for packet 0xF2 (<= 7.0.9.0)\n");
        m_Packets[0xF2].Size = PACKET_VARIABLE_SIZE;
    }

    if (newClientVersion >= CV_70180)
    {
        CVPRINT("Set new length for packet 0x00 (>= 7.0.18.0)\n");
        m_Packets[0x00].Size = 0x6A;
    }
    else
    {
        CVPRINT("Set standart length for packet 0x24 (<= 7.0.18.0)\n");
        m_Packets[0x00].Size = 0x68;
    }
}

int CPacketManager::GetPacketSize(const vector<uint8_t> &packet, int &offsetToSize)
{
    DEBUG_TRACE_FUNCTION;
    if (static_cast<unsigned int>(!packet.empty()) != 0u)
    {
        return m_Packets[packet[0]].Size;
    }

    return 0;
}

void CPacketManager::SendMegaClilocRequests()
{
    DEBUG_TRACE_FUNCTION;
    if (g_TooltipsEnabled && !m_MegaClilocRequests.empty())
    {
        if (g_Config.ClientVersion >= CV_500A)
        {
            while (!m_MegaClilocRequests.empty())
            {
                CPacketMegaClilocRequest(m_MegaClilocRequests).Send();
            }
        }
        else
        {
            for (int i : m_MegaClilocRequests)
            {
                CPacketMegaClilocRequestOld(i).Send();
            }
            m_MegaClilocRequests.clear();
        }
    }
}

void CPacketManager::AddMegaClilocRequest(int serial)
{
    DEBUG_TRACE_FUNCTION;
    for (int item : m_MegaClilocRequests)
    {
        if (item == serial)
        {
            return;
        }
    }

    m_MegaClilocRequests.push_back(serial);
}

void CPacketManager::OnReadFailed()
{
    DEBUG_TRACE_FUNCTION;

    LOG("OnReadFailed...Disconnecting...\n");
    g_Orion.DisconnectGump();
    //g_Orion.Disconnect();
    g_AbyssPacket03First = true;
    g_PluginManager.Disconnect();
    g_ConnectionManager.Disconnect();
}

void CPacketManager::OnPacket()
{
    DEBUG_TRACE_FUNCTION;

    uint32_t ticks = g_Ticks;
    g_TotalRecvSize += (uint32_t)Size;
    CPacketInfo &info = m_Packets[*Start];
    if (info.save)
    {
#if defined(ORION_WINDOWS) // FIXME: localtime_s (use C++ if possible)
        time_t rawtime;
        struct tm timeinfo;
        char buffer[80];

        time(&rawtime);
        localtime_s(&timeinfo, &rawtime);
        strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", &timeinfo);
        LOG("--- ^(%d) r(+%zd => %d) %s Server:: %s\n",
            ticks - g_LastPacketTime,
            Size,
            g_TotalRecvSize,
            buffer,
            info.Name);
#else
        LOG("--- ^(%d) r(+%zd => %d) Server:: %s\n",
            ticks - g_LastPacketTime,
            Size,
            g_TotalRecvSize,
            info.Name);
#endif
        LOG_DUMP(Start, (int)Size);
    }

    g_LastPacketTime = ticks;
    if (info.Direction != DIR_RECV && info.Direction != DIR_BOTH)
    {
        LOG("message direction invalid: 0x%02X\n", *Start);
    }
    else if (g_PluginManager.PacketRecv(Start, (int)Size))
    {
        if (info.Handler != 0)
        {
            Ptr = Start + 1;
            if (info.Size == 0)
            {
                Ptr += 2;
            }
            (this->*(info.Handler))();
        }
    }
}

void CPacketManager::SavePluginReceivePacket(uint8_t *buf, int size)
{
    DEBUG_TRACE_FUNCTION;

    vector<uint8_t> packet(size);
    memcpy(&packet[0], &buf[0], size);

    LOCK(m_Mutex);
    m_PluginData.push_front(packet);
    UNLOCK(m_Mutex);
}

void CPacketManager::ProcessPluginPackets()
{
    DEBUG_TRACE_FUNCTION;

    LOCK(m_Mutex);
    while (!m_PluginData.empty())
    {
        vector<uint8_t> &packet = m_PluginData.back();

        PluginReceiveHandler(&packet[0], (int)packet.size());
        packet.clear();

        m_PluginData.pop_back();
    }
    UNLOCK(m_Mutex);
}

void CPacketManager::PluginReceiveHandler(uint8_t *buf, int size)
{
    DEBUG_TRACE_FUNCTION;
    SetData(buf, size);

    uint32_t ticks = g_Ticks;
    g_TotalRecvSize += (uint32_t)Size;
    CPacketInfo &info = m_Packets[*Start];
    LOG("--- ^(%d) r(+%zd => %d) Plugin->Client:: %s\n",
        ticks - g_LastPacketTime,
        Size,
        g_TotalRecvSize,
        info.Name);
    LOG_DUMP(Start, (int)Size);

    g_LastPacketTime = ticks;
    if (info.Direction != DIR_RECV && info.Direction != DIR_BOTH)
    {
        LOG("message direction invalid: 0x%02X\n", *buf);
    }
    else if (info.Handler != 0)
    {
        Ptr = Start + 1;
        if (info.Size == 0)
        {
            Ptr += 2;
        }
        (this->*(info.Handler))();
    }
}

#define PACKET_HANDLER(name) void CPacketManager::Handle##name()

PACKET_HANDLER(LoginError)
{
    DEBUG_TRACE_FUNCTION;
    if (g_GameState == GS_MAIN_CONNECT || g_GameState == GS_SERVER_CONNECT ||
        g_GameState == GS_GAME_CONNECT)
    {
        g_ConnectionScreen.SetConnectionFailed(true);
        g_ConnectionScreen.SetErrorCode(ReadUInt8());
        g_ConnectionManager.Disconnect();
    }
}

PACKET_HANDLER(ServerList)
{
    DEBUG_TRACE_FUNCTION;
    g_ServerList.ParsePacket(*this);
}

PACKET_HANDLER(RelayServer)
{
    DEBUG_TRACE_FUNCTION;
    memset(&g_SelectedCharName[0], 0, sizeof(g_SelectedCharName));
    in_addr addr;
    uint32_t *paddr = (uint32_t *)Ptr;
    Move(4);
#if defined(ORION_WINDOWS)
    addr.S_un.S_addr = *paddr;
#else
    addr.s_addr = *paddr;
#endif
    char relayIP[30] = { 0 };
    memcpy(&relayIP[0], inet_ntoa(addr), 29);
    int relayPort = ReadUInt16BE();
    g_Orion.RelayServer(relayIP, relayPort, Ptr);
    g_PacketLoginComplete = false;
    g_CurrentMap = 0;
}

PACKET_HANDLER(CharacterList)
{
    DEBUG_TRACE_FUNCTION;

    HandleResendCharacterList();
    uint8_t locCount = ReadUInt8();
    g_CityList.Clear();
    if (g_Config.ClientVersion >= CV_70130)
    {
        for (int i = 0; i < locCount; i++)
        {
            CCityItemNew *city = new CCityItemNew();

            city->LocationIndex = ReadUInt8();

            city->Name = ReadString(32);
            city->Area = ReadString(32);

            city->X = ReadUInt32BE();
            city->Y = ReadUInt32BE();
            city->Z = ReadUInt32BE();
            city->MapIndex = ReadUInt32BE();
            city->Cliloc = ReadUInt32BE();

            Move(4);

            g_CityList.AddCity(city);
        }
    }
    else
    {
        for (int i = 0; i < locCount; i++)
        {
            CCityItem *city = new CCityItem();

            city->LocationIndex = ReadUInt8();

            city->Name = ReadString(31);
            city->Area = ReadString(31);

            city->InitCity();

            g_CityList.AddCity(city);
        }
    }

    g_ClientFlag = ReadUInt32BE();

    g_CharacterList.OnePerson = (bool)(g_ClientFlag & CLF_ONE_CHARACTER_SLOT);
    //g_SendLogoutNotification = (bool)(g_ClientFlag & LFF_RE);
    g_PopupEnabled = (bool)(g_ClientFlag & CLF_CONTEXT_MENU);
    g_TooltipsEnabled =
        (bool)(((g_ClientFlag & CLF_PALADIN_NECROMANCER_TOOLTIPS) != 0u) && (g_Config.ClientVersion >= CV_308Z));
    g_PaperdollBooks = (bool)(g_ClientFlag & CLF_PALADIN_NECROMANCER_TOOLTIPS);

    g_CharacterListScreen.UpdateContent();
}

PACKET_HANDLER(ResendCharacterList)
{
    DEBUG_TRACE_FUNCTION;
    g_Orion.InitScreen(GS_CHARACTER);

    int numSlots = ReadInt8();
    if (*Start == 0x86)
    {
        LOG("/======Resend chars===\n");
    }
    else
    {
        LOG("/======Chars===\n");
    }

    g_CharacterList.Clear();
    g_CharacterList.Count = numSlots;

    int autoPos = -1;
    bool autoLogin = g_MainScreen.m_AutoLogin->Checked;
    bool haveCharacter = false;

    if (numSlots == 0)
    {
        LOG("Warning!!! No slots in character list\n");
    }
    else
    {
        int selectedPos = -1;
        for (int i = 0; i < numSlots; i++)
        {
            string name = ReadString(30);
            Move(30);
            if (name.length() != 0u)
            {
                haveCharacter = true;
                g_CharacterList.SetName(i, name);

                if (autoLogin && autoPos == -1 && AutoLoginNameExists(name))
                {
                    autoPos = i;
                }

                if (name == g_CharacterList.LastCharacterName)
                {
                    g_CharacterList.Selected = i;
                    if (autoLogin && selectedPos == -1)
                    {
                        selectedPos = i;
                    }
                }
            }

            LOG("%d: %s (%zd)\n", i, name.c_str(), name.length());
        }

        if (autoLogin && autoPos == -1)
        {
            autoPos = selectedPos;
        }
    }

    if (autoLogin && (numSlots != 0))
    {
        if (autoPos == -1)
        {
            autoPos = 0;
        }

        g_CharacterList.Selected = autoPos;
        if (g_CharacterList.GetName(autoPos).length() != 0u)
        {
            g_Orion.CharacterSelection(autoPos);
        }
    }

    if (*Start == 0x86)
    {
        g_CharacterListScreen.UpdateContent();
    }

    if (!haveCharacter)
    {
        g_Orion.InitScreen(GS_PROFESSION_SELECT);
    }
}

PACKET_HANDLER(LoginComplete)
{
    DEBUG_TRACE_FUNCTION;
    g_PacketLoginComplete = true;
    g_Orion.LoginComplete(false);
}

PACKET_HANDLER(SetTime)
{
    DEBUG_TRACE_FUNCTION;
    g_ServerTimeHour = ReadUInt8();
    g_ServerTimeMinute = ReadUInt8();
    g_ServerTimeSecond = ReadUInt8();
}

PACKET_HANDLER(EnterWorld)
{
    DEBUG_TRACE_FUNCTION;
    uint32_t serial = ReadUInt32BE();
    ConfigSerial = serial;
    bool loadConfig = false;

    if (g_World != nullptr)
    {
        LOG("Warning!!! Duplicate enter world message\n");

        g_Orion.SaveLocalConfig(g_PacketManager.ConfigSerial);
        ConfigSerial = g_PlayerSerial;
        g_ConfigLoaded = false;
        loadConfig = true;
    }

    g_Orion.ClearWorld();

    g_World = new CGameWorld(serial);

    Move(4); //unused

    if (strlen(g_SelectedCharName) != 0u)
    {
        g_Player->SetName(g_SelectedCharName);
    }

    g_Player->Graphic = ReadUInt16BE();
    g_Player->OnGraphicChange();

    g_Player->SetX(ReadUInt16BE());
    g_Player->SetY(ReadUInt16BE());
    g_Player->SetZ((char)ReadUInt16BE());
    g_Player->Direction = ReadUInt8();
    /*Move(1); //serverID
	Move(4); //unused
	Move(2); //serverBoundaryX
	Move(2); //serverBoundaryY
	Move(2); //serverBoundaryWidth
	Move(2); //serverBoundaryHeight*/

    g_RemoveRangeXY.X = g_Player->GetX();
    g_RemoveRangeXY.Y = g_Player->GetY();

    UOI_PLAYER_XYZ_DATA xyzData = { g_RemoveRangeXY.X, g_RemoveRangeXY.Y, 0 };
    PLUGIN_EVENT(UOMSG_UPDATE_REMOVE_POS, &xyzData);

    g_Player->OffsetX = 0;
    g_Player->OffsetY = 0;
    g_Player->OffsetZ = 0;

    LOG("Player 0x%08lX entered the world.\n", serial);

    g_MapManager.Init();
    g_MapManager.AddRender(g_Player);

    g_Orion.LoadStartupConfig(ConfigSerial);

    g_LastSpellIndex = 1;
    g_LastSkillIndex = 1;

    CPacketClientVersion(g_Config.ClientVersionString).Send();

    if (g_Config.ClientVersion >= CV_200)
    {
        CPacketGameWindowSize().Send();
    }

    if (g_Config.ClientVersion >= CV_200)
    {
        CPacketLanguage(g_Language).Send();
    }

    g_Orion.Click(g_PlayerSerial);
    CPacketStatusRequest(g_PlayerSerial).Send();

    if (g_Player->Dead())
    {
        g_Orion.ChangeSeason(ST_DESOLATION, DEATH_MUSIC_INDEX);
    }

    if (loadConfig)
    {
        g_Orion.LoginComplete(true);
    }
    else
    {
        CServer *server = g_ServerList.GetSelectedServer();
        if (server != nullptr)
        {
            g_Orion.CreateTextMessageF(3, 0x0037, "Login confirm to %s", server->Name.c_str());
        }
    }
}

PACKET_HANDLER(UpdateHitpoints)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    CGameCharacter *obj = g_World->FindWorldCharacter(serial);
    if (obj == nullptr)
    {
        return;
    }

    obj->MaxHits = ReadInt16BE();
    obj->Hits = ReadInt16BE();

    g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);
    g_GumpManager.UpdateContent(serial, 0, GT_TARGET_SYSTEM);
}

PACKET_HANDLER(UpdateMana)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();

    CGameCharacter *obj = g_World->FindWorldCharacter(serial);
    if (obj == nullptr)
    {
        return;
    }

    obj->MaxMana = ReadInt16BE();
    obj->Mana = ReadInt16BE();
    g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);
}

PACKET_HANDLER(UpdateStamina)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();

    CGameCharacter *obj = g_World->FindWorldCharacter(serial);
    if (obj == nullptr)
    {
        return;
    }

    obj->MaxStam = ReadInt16BE();
    obj->Stam = ReadInt16BE();
    g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);
}

PACKET_HANDLER(MobileAttributes)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();

    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    if (obj == nullptr)
    {
        return;
    }

    obj->MaxHits = ReadInt16BE();
    obj->Hits = ReadInt16BE();

    obj->MaxMana = ReadInt16BE();
    obj->Mana = ReadInt16BE();

    obj->MaxStam = ReadInt16BE();
    obj->Stam = ReadInt16BE();

    g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);
    g_GumpManager.UpdateContent(serial, 0, GT_TARGET_SYSTEM);
}

PACKET_HANDLER(NewHealthbarUpdate)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    if (*Start == 0x16 && g_Config.ClientVersion < CV_500A)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    CGameCharacter *obj = g_World->FindWorldCharacter(serial);
    if (obj == nullptr)
    {
        return;
    }

    uint16_t count = ReadUInt16BE();
    for (int i = 0; i < count; i++)
    {
        uint16_t type = ReadUInt16BE();
        uint8_t enable = ReadUInt8(); //enable/disable
        uint8_t flags = obj->GetFlags();
        if (type == 1) //Poison, enable as poisonlevel + 1
        {
            uint8_t poisonFlag = 0x04;
            if (enable != 0u)
            {
                if (g_Config.ClientVersion >= CV_7000)
                {
                    obj->SA_Poisoned = true;
                }
                else
                {
                    flags |= poisonFlag;
                }
            }
            else
            {
                if (g_Config.ClientVersion >= CV_7000)
                {
                    obj->SA_Poisoned = false;
                }
                else
                {
                    flags &= ~poisonFlag;
                }
            }
        }
        else if (type == 2) //Yellow hits
        {
            if (enable != 0u)
            {
                flags |= 0x08;
            }
            else
            {
                flags &= ~0x08;
            }
        }
        else if (type == 3) //Red?
        {
        }

        obj->SetFlags(flags);
    }

    g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);
}

PACKET_HANDLER(UpdatePlayer)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    uint16_t graphic = ReadUInt16BE();
    uint8_t graphicIncrement = ReadUInt8();
    uint16_t color = ReadUInt16BE();
    uint8_t flags = ReadUInt8();
    uint16_t x = ReadUInt16BE();
    uint16_t y = ReadUInt16BE();
    uint16_t serverID = ReadUInt16BE();
    uint8_t direction = ReadUInt8();
    char z = ReadUInt8();

    // Invert character wakthrough bit.
    flags ^= 0x10;

    LOG("0x%08X 0x%04X %i 0x%04X 0x%02X %i %i %i %i %i\n",
        serial,
        graphic,
        graphicIncrement,
        color,
        flags,
        x,
        y,
        serverID,
        direction,
        z);
    g_World->UpdatePlayer(
        serial, graphic, graphicIncrement, color, flags, x, y, serverID, direction, z);
}

PACKET_HANDLER(CharacterStatus)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();

    CGameCharacter *obj = g_World->FindWorldCharacter(serial);
    if (obj == nullptr)
    {
        return;
    }

    string name = ReadString(30);
    obj->SetName(name);

    obj->Hits = ReadInt16BE();
    obj->MaxHits = ReadInt16BE();

    obj->CanChangeName = (ReadUInt8() != 0);

    uint8_t flag = ReadUInt8();

    if (flag > 0)
    {
        obj->Female = (ReadUInt8() != 0); //buf[43];

        if (serial == g_PlayerSerial)
        {
            short newStr = ReadInt16BE();
            short newDex = ReadInt16BE();
            short newInt = ReadInt16BE();

            if (g_ConfigManager.StatReport && (g_Player->Str != 0))
            {
                short currentStr = g_Player->Str;
                short currentDex = g_Player->Dex;
                short currentInt = g_Player->Int;

                short deltaStr = newStr - currentStr;
                short deltaDex = newDex - currentDex;
                short deltaInt = newInt - currentInt;

                char str[64] = { 0 };
                if (deltaStr != 0)
                {
                    sprintf_s(
                        str, "Your strength has changed by %d.  It is now %d.", deltaStr, newStr);
                    g_Orion.CreateTextMessage(TT_SYSTEM, 0, 3, 0x0170, str);
                }

                if (deltaDex != 0)
                {
                    sprintf_s(
                        str, "Your dexterity has changed by %d.  It is now %d.", deltaDex, newDex);
                    g_Orion.CreateTextMessage(TT_SYSTEM, 0, 3, 0x0170, str);
                }

                if (deltaInt != 0)
                {
                    sprintf_s(
                        str,
                        "Your intelligence has changed by %d.  It is now %d.",
                        deltaInt,
                        newInt);
                    g_Orion.CreateTextMessage(TT_SYSTEM, 0, 3, 0x0170, str);
                }
            }

            g_Player->Str = newStr;
            g_Player->Dex = newDex;
            g_Player->Int = newInt;

            g_Player->Stam = ReadInt16BE();
            g_Player->MaxStam = ReadInt16BE();
            g_Player->Mana = ReadInt16BE();
            g_Player->MaxMana = ReadInt16BE();
            g_Player->Gold = ReadUInt32BE();
            g_Player->Armor = ReadInt16BE();
            g_Player->Weight = ReadInt16BE(); //+64

            if (flag >= 5)
            {
                g_Player->MaxWeight = ReadInt16BE(); //unpack16(buf + 66);
                uint32_t race = ReadUInt8();

                if (race == 0u)
                {
                    race = 1;
                }

                g_Player->Race = (RACE_TYPE)race;
            }
            else
            {
                if (g_Config.ClientVersion >= CV_500A)
                {
                    g_Player->MaxWeight = 7 * (g_Player->Str / 2) + 40;
                }
                else
                {
                    g_Player->MaxWeight = (g_Player->Str * 4) + 25;
                }
            }

            if (flag >= 3)
            {
                g_Player->StatsCap = ReadUInt16BE();
                g_Player->Followers = ReadUInt8();
                g_Player->MaxFollowers = ReadUInt8();
            }

            if (flag >= 4)
            {
                g_Player->FireResistance = ReadInt16BE();
                g_Player->ColdResistance = ReadInt16BE();
                g_Player->PoisonResistance = ReadInt16BE();
                g_Player->EnergyResistance = ReadInt16BE();
                g_Player->Luck = ReadInt16BE();
                g_Player->MinDamage = ReadInt16BE();
                g_Player->MaxDamage = ReadInt16BE();
                g_Player->TithingPoints = ReadUInt32BE();
            }

            if (flag >= 6)
            {
                g_Player->MaxPhysicalResistance = ReadInt16BE();
                g_Player->MaxFireResistance = ReadInt16BE();
                g_Player->MaxColdResistance = ReadInt16BE();
                g_Player->MaxPoisonResistance = ReadInt16BE();
                g_Player->MaxEnergyResistance = ReadInt16BE();
                g_Player->DefenceChance = ReadInt16BE();
                g_Player->MaxDefenceChance = ReadInt16BE();
                g_Player->AttackChance = ReadInt16BE();
                g_Player->WeaponSpeed = ReadInt16BE();
                g_Player->WeaponDamage = ReadInt16BE();
                g_Player->LowerRegCost = ReadInt16BE();
                g_Player->SpellDamage = ReadInt16BE();
                g_Player->CastRecovery = ReadInt16BE();
                g_Player->CastSpeed = ReadInt16BE();
                g_Player->LowerManaCost = ReadInt16BE();
            }

            if (!g_ConnectionScreen.GetCompleted() && g_PacketLoginComplete)
            {
                g_Orion.LoginComplete(false);
            }
        }
    }

    g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);
    g_GumpManager.UpdateContent(serial, 0, GT_TARGET_SYSTEM);
}

PACKET_HANDLER(UpdateItem)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();

    if (serial == g_PlayerSerial)
    {
        return;
    }

    UPDATE_GAME_OBJECT_TYPE updateType = UGOT_ITEM;
    uint16_t count = 0;
    uint8_t graphicIncrement = 0;
    uint8_t direction = 0;
    uint16_t color = 0;
    uint8_t flags = 0;

    if ((serial & 0x80000000) != 0u)
    {
        serial &= 0x7FFFFFFF;
        count = 1;
    }

    uint16_t graphic = ReadUInt16BE();

    if (g_Config.TheAbyss && (graphic & 0x7FFF) == 0x0E5C)
    {
        return;
    }

    if ((graphic & 0x8000) != 0)
    {
        graphic &= 0x7FFF;
        graphicIncrement = ReadUInt8();
    }

    if (count != 0u)
    {
        count = ReadUInt16BE();
    }
    else
    {
        count++;
    }

    uint16_t x = ReadUInt16BE();

    if ((x & 0x8000) != 0)
    {
        x &= 0x7FFF;
        direction = 1;
    }

    uint16_t y = ReadUInt16BE();

    if ((y & 0x8000) != 0)
    {
        y &= 0x7FFF;
        color = 1;
    }

    if ((y & 0x4000) != 0)
    {
        y &= 0x3FFF;
        flags = 1;
    }

    if (direction != 0u)
    {
        direction = ReadUInt8();
    }

    char z = ReadUInt8();

    if (color != 0u)
    {
        color = ReadUInt16BE();
    }

    if (flags != 0u)
    {
        flags = ReadUInt8();
    }

    if (graphic >= 0x4000)
    {
        //graphic += 0xC000;
        //updateType = UGOT_NEW_ITEM;
        updateType = UGOT_MULTI;
    }

    g_World->UpdateGameObject(
        serial,
        graphic,
        graphicIncrement,
        count,
        x,
        y,
        z,
        direction,
        color,
        flags,
        count,
        updateType,
        1);
}

PACKET_HANDLER(UpdateItemSA)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    Move(2);
    UPDATE_GAME_OBJECT_TYPE updateType = (UPDATE_GAME_OBJECT_TYPE)ReadUInt8();
    uint32_t serial = ReadUInt32BE();
    uint16_t graphic = ReadUInt16BE();
    uint8_t graphicIncrement = ReadUInt8();
    uint16_t count = ReadUInt16BE();
    uint16_t unknown = ReadUInt16BE();
    uint16_t x = ReadUInt16BE();
    uint16_t y = ReadUInt16BE();
    uint8_t z = ReadUInt8();
    uint8_t direction = ReadUInt8();
    uint16_t color = ReadUInt16BE();
    uint8_t flags = ReadUInt8();
    uint16_t unknown2 = ReadUInt16BE();

    if (serial != g_PlayerSerial)
    {
        g_World->UpdateGameObject(
            serial,
            graphic,
            graphicIncrement,
            count,
            x,
            y,
            z,
            direction,
            color,
            flags,
            unknown,
            updateType,
            unknown2);
    }
    else if (*Start == 0xF7)
    { //из пакета 0xF7 для игрока определенная обработка
        g_World->UpdatePlayer(
            serial, graphic, graphicIncrement, color, flags, x, y, 0 /*serverID*/, direction, z);
    }
}

PACKET_HANDLER(UpdateObject)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    uint16_t graphic = ReadUInt16BE();
    uint16_t x = ReadUInt16BE();
    uint16_t y = ReadUInt16BE();
    uint8_t z = ReadUInt8();
    uint8_t direction = ReadUInt8();
    uint16_t color = ReadUInt16BE();
    uint8_t flags = ReadUInt8();
    uint8_t notoriety = ReadUInt8();
    bool oldDead = false;

    bool isAlreadyExists = (g_World->FindWorldObject(serial) != nullptr);

    if (serial == g_PlayerSerial)
    {
        if (g_Player != nullptr)
        {
            bool updateStatusbar = (g_Player->GetFlags() != flags);

            oldDead = g_Player->Dead();
            g_Player->Graphic = graphic;
            g_Player->OnGraphicChange(1000);
            g_Player->Color = g_ColorManager.FixColor(color);
            g_Player->SetFlags(flags);

            if (updateStatusbar)
            {
                g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);
            }
        }
    }
    else
    {
        g_World->UpdateGameObject(
            serial, graphic, 0, 0, x, y, z, direction, color, flags, 0, UGOT_ITEM, 1);
    }

    CGameObject *obj = g_World->FindWorldObject(serial);

    if (obj == nullptr)
    {
        return;
    }

    obj->ClearNotOpenedItems();

    if (obj->NPC)
    {
        ((CGameCharacter *)obj)->Notoriety = notoriety;
        g_GumpManager.UpdateContent(serial, 0, GT_PAPERDOLL);
    }

    if (*Start != 0x78)
    {
        Move(6);
    }

    uint32_t itemSerial = ReadUInt32BE();

    while (itemSerial != 0 && !IsEOF())
    {
        uint16_t itemGraphic = ReadUInt16BE();
        uint8_t layer = ReadUInt8();
        uint16_t itemColor = 0;

        if (g_Config.ClientVersion >= CV_70331)
        {
            itemColor = ReadUInt16BE();
        }
        else if ((itemGraphic & 0x8000) != 0)
        {
            itemGraphic &= 0x7FFF;
            itemColor = ReadUInt16BE();
        }

        CGameItem *item = g_World->GetWorldItem(itemSerial);

        item->MapIndex = g_CurrentMap;

        item->Graphic = itemGraphic;
        item->Color = g_ColorManager.FixColor(itemColor);

        g_World->PutEquipment(item, obj, layer);
        item->OnGraphicChange();

        LOG("\t0x%08X:%04X [%d] %04X\n", item->Serial, item->Graphic, layer, item->Color);

        g_World->MoveToTop(item);

        itemSerial = ReadUInt32BE();
    }

    if (obj->IsPlayer())
    {
        if (oldDead != g_Player->Dead())
        {
            if (g_Player->Dead())
            {
                g_Orion.ChangeSeason(ST_DESOLATION, DEATH_MUSIC_INDEX);
            }
            else
            {
                g_Orion.ChangeSeason(g_OldSeason, g_OldSeasonMusic);
            }
        }

        g_GumpManager.UpdateContent(serial, 0, GT_PAPERDOLL);

        g_Player->UpdateAbilities();
    }
}

PACKET_HANDLER(EquipItem)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();

    CGameItem *obj = g_World->GetWorldItem(serial);
    obj->MapIndex = g_CurrentMap;

    if ((obj->Graphic != 0u) && obj->Layer != OL_BACKPACK)
    {
        obj->Clear();
    }

    obj->Graphic = ReadUInt16BE();
    Move(1);
    int layer = ReadUInt8();
    uint32_t cserial = ReadUInt32BE();
    obj->Color = g_ColorManager.FixColor(ReadUInt16BE());

    if (obj->Container != 0xFFFFFFFF)
    {
        g_GumpManager.UpdateContent(obj->Container, 0, GT_CONTAINER);
        g_GumpManager.UpdateContent(obj->Container, 0, GT_PAPERDOLL);
    }

    g_World->PutEquipment(obj, cserial, layer);
    obj->OnGraphicChange();

    if (g_NewTargetSystem.Serial == serial)
    {
        g_NewTargetSystem.Serial = 0;
    }

    if (layer >= OL_BUY_RESTOCK && layer <= OL_SELL)
    {
        obj->Clear();
    }
    else if (layer < OL_MOUNT)
    {
        g_GumpManager.UpdateContent(cserial, 0, GT_PAPERDOLL);
    }

    if (cserial == g_PlayerSerial && (layer == OL_1_HAND || layer == OL_2_HAND))
    {
        g_Player->UpdateAbilities();
    }
}

PACKET_HANDLER(UpdateContainedItem)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    uint16_t graphic = ReadUInt16BE();
    uint8_t graphicIncrement = ReadUInt8();
    uint16_t count = ReadUInt16BE();
    uint16_t x = ReadUInt16BE();
    uint16_t y = ReadUInt16BE();

    if (g_Config.ClientVersion >= CV_6017)
    {
        Move(1);
    }

    uint32_t containerSerial = ReadUInt32BE();
    uint16_t color = ReadUInt16BE();

    g_World->UpdateContainedItem(
        serial, graphic, graphicIncrement, count, x, y, containerSerial, color);
}

PACKET_HANDLER(UpdateContainedItems)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint16_t itemsCount = ReadUInt16BE();

    for (int i = 0; i < itemsCount; i++)
    {
        uint32_t serial = ReadUInt32BE();
        uint16_t graphic = ReadUInt16BE();
        uint8_t graphicIncrement = ReadUInt8();
        uint16_t count = ReadUInt16BE();
        uint16_t x = ReadUInt16BE();
        uint16_t y = ReadUInt16BE();

        if (g_Config.ClientVersion >= CV_6017)
        {
            Move(1);
        }

        uint32_t containerSerial = ReadUInt32BE();
        uint16_t color = ReadUInt16BE();

        if (i == 0)
        {
            CGameObject *container = g_World->FindWorldObject(containerSerial);

            if (container != nullptr)
            {
                LOG("Making %08X empty...\n", containerSerial);

                if (container->IsCorpse())
                {
                    container->ClearUnequipped();
                }
                else
                {
                    container->Clear();
                }

                /*if ((*(int(__thiscall **)(CGameContainer *))((int(__thiscall **)(_DWORD))containerObj1->GameObject.VTable
					+ 12))(containerObj1))
				{
					if (containerObj1->GameObject.Parent)
					{
						if ((*((int(**)(void))containerObj1->GameObject.Parent->VTable + 9))())
						{
							v3 = containerObj1->GameObject.Parent;
							if (v3->field_B8)
							{
								if ((*(int(**)(void))(*(_DWORD *)v3->field_B8 + 380))())
								{
									v4 = containerObj1->GameObject.Parent->field_B8;
									if (v4)
										(**(void(__stdcall ***)(_DWORD))v4)(1);
								}
							}
						}
					}
					sub_5A6FE0(containerObj1);
				}*/
            }
        }

        g_World->UpdateContainedItem(
            serial, graphic, graphicIncrement, count, x, y, containerSerial, color);
    }
}

PACKET_HANDLER(DenyMoveItem)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    if (g_ObjectInHand.Enabled ||
        (g_ObjectInHand.Dropped && g_World->FindWorldItem(g_ObjectInHand.Serial) == nullptr))
    {
        if (g_World->ObjectToRemove == g_ObjectInHand.Serial)
        {
            g_World->ObjectToRemove = 0;
        }

        if (!g_ObjectInHand.UpdatedInWorld)
        {
            if ((g_ObjectInHand.Layer == 0u) && (g_ObjectInHand.Container != 0u) &&
                g_ObjectInHand.Container != 0xFFFFFFFF)
            {
                g_World->UpdateContainedItem(
                    g_ObjectInHand.Serial,
                    g_ObjectInHand.Graphic,
                    0,
                    g_ObjectInHand.TotalCount,
                    g_ObjectInHand.X,
                    g_ObjectInHand.Y,
                    g_ObjectInHand.Container,
                    g_ObjectInHand.Color);

                g_GumpManager.UpdateContent(g_ObjectInHand.Container, 0, GT_CONTAINER);
            }
            else
            {
                CGameItem *obj = g_World->GetWorldItem(g_ObjectInHand.Serial);

                if (obj != nullptr)
                {
                    obj->Graphic = g_ObjectInHand.Graphic;
                    obj->Color = g_ObjectInHand.Color;
                    obj->Count = g_ObjectInHand.TotalCount;
                    obj->SetFlags(g_ObjectInHand.Flags);
                    obj->SetX(g_ObjectInHand.X);
                    obj->SetY(g_ObjectInHand.Y);
                    obj->SetZ(g_ObjectInHand.Z);
                    obj->OnGraphicChange();

                    CGameObject *container = g_World->FindWorldObject(g_ObjectInHand.Container);

                    if (container != nullptr)
                    {
                        if (container->NPC)
                        {
                            g_World->PutEquipment(obj, container, g_ObjectInHand.Layer);

                            g_GumpManager.UpdateContent(obj->Container, 0, GT_PAPERDOLL);
                        }
                        else
                        {
                            g_World->RemoveObject(obj);
                            obj = nullptr;
                        }
                    }
                    else
                    {
                        g_World->RemoveFromContainer(obj);
                    }

                    if (obj != nullptr)
                    {
                        g_World->MoveToTop(obj);
                    }
                }
            }
        }

        g_GumpManager.CloseGump(g_ObjectInHand.Serial, 0, GT_DRAG);

        g_ObjectInHand.Clear();
    }

    uint8_t code = ReadUInt8();

    if (code < 5)
    {
        const string errorMessages[] = {
            "You can not pick that up.",
            "That is too far away.",
            "That is out of sight.",
            "That item does not belong to you.  You'll have to steal it.",
            "You are already holding an item."
        };

        g_Orion.CreateTextMessage(TT_SYSTEM, 0xFFFFFFFF, 3, 0, errorMessages[code]);
    }
}

PACKET_HANDLER(EndDraggingItem)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    //Unused
    //Move(2);
    //Move(2);

    g_ObjectInHand.Enabled = false;
    g_ObjectInHand.Dropped = false;
}

PACKET_HANDLER(DropItemAccepted)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    g_ObjectInHand.Enabled = false;
    g_ObjectInHand.Dropped = false;
}

PACKET_HANDLER(DeleteObject)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();

    if (serial == g_PlayerSerial)
    {
        return;
    }

    CGameObject *obj = g_World->FindWorldObject(serial);

    if (obj != nullptr)
    {
        bool updateAbilities = false;
        uint32_t cont = obj->Container & 0x7FFFFFFF;

        if (obj->Container != 0xFFFFFFFF)
        {
            CGameObject *top = obj->GetTopObject();

            if (top != nullptr)
            {
                if (top->IsPlayer())
                {
                    CGameItem *item = (CGameItem *)obj;
                    updateAbilities = (item->Layer == OL_1_HAND || item->Layer == OL_2_HAND);
                }

                CGameObject *tradeBox = top->FindSecureTradeBox();

                if (tradeBox != nullptr)
                {
                    g_GumpManager.UpdateContent(0, tradeBox->Serial, GT_TRADE);
                }
            }

            if (cont == g_PlayerSerial && ((CGameItem *)obj)->Layer == OL_NONE)
            {
                g_ObjectInHand.Enabled = false;
            }

            if (!obj->NPC && ((CGameItem *)obj)->Layer != OL_NONE)
            {
                g_GumpManager.UpdateContent(cont, 0, GT_PAPERDOLL);
            }

            CGump *gump = g_GumpManager.UpdateContent(cont, 0, GT_CONTAINER);

            if (obj->Graphic == 0x0EB0)
            {
                g_GumpManager.CloseGump(serial, cont, GT_BULLETIN_BOARD_ITEM);

                CGumpBulletinBoard *bbGump =
                    (CGumpBulletinBoard *)g_GumpManager.UpdateGump(cont, 0, GT_BULLETIN_BOARD);

                if (bbGump != nullptr && bbGump->m_HTMLGump != nullptr)
                {
                    QFOR(go, bbGump->m_HTMLGump->m_Items, CBaseGUI *)
                    {
                        if (go->Serial == serial)
                        {
                            bbGump->m_HTMLGump->Delete(go);

                            int posY = 0;

                            QFOR(go1, bbGump->m_HTMLGump->m_Items, CBaseGUI *)
                            {
                                if (go1->Type == GOT_BB_OBJECT)
                                {
                                    go1->SetY(posY);
                                    posY += 18;
                                }
                            }

                            bbGump->m_HTMLGump->CalculateDataSize();

                            break;
                        }
                    }
                }
            }
        }

        if (!g_CorpseManager.InList(0, serial))
        {
            if (obj->NPC)
            {
                if (g_Party.Contains(obj->Serial))
                {
                    g_GumpManager.UpdateContent(obj->Serial, 0, GT_STATUSBAR);

                    obj->RemoveRender();
                }
                else
                {
                    g_World->RemoveObject(obj);
                }
            }
            else
            {
                g_World->RemoveObject(obj);

                if (updateAbilities)
                {
                    g_Player->UpdateAbilities();
                }
            }
        }
    }
}

PACKET_HANDLER(UpdateCharacter)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    if (obj == nullptr)
    {
        return;
    }

    uint16_t graphic = ReadUInt16BE();
    uint16_t x = ReadUInt16BE();
    uint16_t y = ReadUInt16BE();
    char z = ReadUInt8();
    uint8_t direction = ReadUInt8();
    uint16_t color = ReadUInt16BE();
    uint8_t flags = ReadUInt8();
    uint8_t notoriety = ReadUInt8();

    // Bug #78
    // Outlands server somewhat sends back what seems to be invalid direction data
    // Looking at RunUO source, mobile directions flag is valid to be at most 0x87
    // https://github.com/runuo/runuo/blob/d715573172fc432a673825b0136444bdab7863b5/Server/Mobile.cs#L390-L405
    // But in Outlands when a Mobile has low HP and start running away, if the player
    // forces it to change direction by circling it, eventually a bad packet with a
    // direction of 0x08 will come in.
    if ((direction & 0x87) != direction)
    {
        LOG("Clamping invalid/unknown direction: %d\n", direction);
        direction &= 0x87;
    }

    obj->Notoriety = notoriety;

    if (obj->IsPlayer())
    {
        obj->SetFlags(flags);
    }
    else
    {
        if (!obj->m_Steps.empty() && obj->Direction == obj->m_Steps.back().Direction)
        {
            CWalkData &wd = obj->m_Steps.back();

            obj->SetX(wd.X);
            obj->SetY(wd.Y);
            obj->SetZ(wd.Z);
            obj->Direction = wd.Direction;

            obj->m_Steps.clear();
        }

        g_World->UpdateGameObject(
            serial, graphic, 0, 0, x, y, z, direction, color, flags, 0, UGOT_ITEM, 1);
    }

    g_World->MoveToTop(obj);
}

PACKET_HANDLER(Warmode)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    g_Player->Warmode = (ReadUInt8() != 0u);

    g_GumpManager.UpdateContent(g_PlayerSerial, 0, GT_STATUSBAR);

    CGumpPaperdoll *gump = (CGumpPaperdoll *)g_GumpManager.GetGump(g_PlayerSerial, 0, GT_PAPERDOLL);

    if (gump != nullptr && gump->m_ButtonWarmode != nullptr)
    {
        uint16_t graphic = 0x07E5;

        if (g_Player->Warmode)
        {
            graphic += 3;
        }

        gump->m_ButtonWarmode->Graphic = graphic;
        gump->m_ButtonWarmode->GraphicSelected = graphic + 2;
        gump->m_ButtonWarmode->GraphicPressed = graphic + 1;

        gump->WantRedraw = true;
    }

    g_World->MoveToTop(g_Player);
}

PACKET_HANDLER(PauseControl)
{
    DEBUG_TRACE_FUNCTION;
    /*g_ClientPaused = ReadUInt8();

	if (!g_ClientPaused)
	UO->ResumeClient();*/
}

PACKET_HANDLER(OpenPaperdoll)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();

    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    string text = ReadString(60);

    uint8_t flags = ReadUInt8();

    if (obj != nullptr)
    {
        obj->Title = text;
    }

    CGumpPaperdoll *gump = (CGumpPaperdoll *)g_GumpManager.UpdateGump(serial, 0, GT_PAPERDOLL);

    if (gump == nullptr)
    {
        gump = new CGumpPaperdoll(serial, 0, 0, false);
        g_GumpManager.AddGump(gump);
    }

    gump->CanLift = ((flags & 0x02) != 0);
    gump->UpdateDescription(text);
}

PACKET_HANDLER(ClientVersion)
{
    DEBUG_TRACE_FUNCTION;
    CPacketClientVersion(g_Config.ClientVersionString).Send();
}

PACKET_HANDLER(Ping)
{
    DEBUG_TRACE_FUNCTION;
    g_Ping = 0;
}

PACKET_HANDLER(SetWeather)
{
    DEBUG_TRACE_FUNCTION;
    g_Weather.Reset();

    uint8_t type = ReadUInt8();
    g_Weather.Type = type;
    g_Weather.Count = ReadUInt8();

    bool showMessage = (g_Weather.Count > 0);

    if (g_Weather.Count > 70)
    {
        g_Weather.Count = 70;
    }

    g_Weather.Temperature = ReadUInt8();
    g_Weather.Timer = g_Ticks + WEATHER_TIMER;
    g_Weather.Generate();

    switch (type)
    {
        case 0:
        {
            if (showMessage)
            {
                g_Orion.CreateTextMessage(TT_SYSTEM, 0xFFFFFFFF, 3, 0, "It begins to rain.");
            }

            break;
        }
        case 1:
        {
            if (showMessage)
            {
                g_Orion.CreateTextMessage(
                    TT_SYSTEM, 0xFFFFFFFF, 3, 0, "A fierce storm approaches.");
            }

            break;
        }
        case 2:
        {
            if (showMessage)
            {
                g_Orion.CreateTextMessage(TT_SYSTEM, 0xFFFFFFFF, 3, 0, "It begins to snow.");
            }

            break;
        }
        case 3:
        {
            if (showMessage)
            {
                g_Orion.CreateTextMessage(TT_SYSTEM, 0xFFFFFFFF, 3, 0, "A storm is brewing.");
            }

            break;
        }
        case 0xFE:
        case 0xFF:
        {
            g_Weather.Timer = 0;
            break;
        }
        default:
            break;
    }
}

PACKET_HANDLER(PersonalLightLevel)
{
    DEBUG_TRACE_FUNCTION;
    uint32_t serial = ReadUInt32BE();

    if (serial == g_PlayerSerial)
    {
        uint8_t level = ReadUInt8();

        if (level > 0x1F)
        {
            level = 0x1F;
        }

        g_PersonalLightLevel = level;
    }
}

PACKET_HANDLER(LightLevel)
{
    DEBUG_TRACE_FUNCTION;
    uint8_t level = ReadUInt8();

    if (level > 0x1F)
    {
        level = 0x1F;
    }

    g_LightLevel = level;
}

PACKET_HANDLER(EnableLockedFeatures)
{
    DEBUG_TRACE_FUNCTION;
    if (g_Config.ClientVersion >= CV_60142)
    {
        g_LockedClientFeatures = ReadUInt32BE();
    }
    else
    {
        g_LockedClientFeatures = ReadUInt16BE();
    }

    g_ChatEnabled = (bool)(g_LockedClientFeatures & LFF_T2A);

    g_AnimationManager.UpdateAnimationAddressTable();
}

PACKET_HANDLER(OpenContainer)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    uint16_t gumpid = ReadUInt16BE();

    CGump *gump = nullptr;

    if (gumpid == 0xFFFF) //Spellbook
    {
        int gameWindowCenterX =
            (g_ConfigManager.GameWindowX - 4) + g_ConfigManager.GameWindowWidth / 2;
        int gameWindowCenterY =
            (g_ConfigManager.GameWindowY - 4) + g_ConfigManager.GameWindowHeight / 2;

        int x = gameWindowCenterX - 200;
        int y = gameWindowCenterY - 100;

        if (x < 0)
        {
            x = 0;
        }

        if (y < 0)
        {
            y = 0;
        }

        gump = new CGumpSpellbook(serial, x, y);
        g_Orion.PlaySoundEffect(0x0055);
    }
    else if (gumpid == 0x0030) //Buylist
    {
        g_GumpManager.CloseGump(serial, 0, GT_SHOP);

        CGameCharacter *vendor = g_World->FindWorldCharacter(serial);

        if (vendor != nullptr)
        {
            CGumpShop *buyGump = new CGumpShop(serial, true, 150, 5);
            gump = buyGump;
            buyGump->Visible = true;

            for (int layer = OL_BUY_RESTOCK; layer < OL_BUY + 1; layer++)
            {
                CGameItem *item = vendor->FindLayer((int)layer);

                if (item == nullptr)
                {
                    LOG("Buy layer %i not found!\n", layer);
                    continue;
                }

                item = (CGameItem *)item->m_Items;

                if (item == nullptr)
                {
                    LOG("Buy items not found!\n");
                    continue;
                }

                bool reverse = (item->GetX() > 1);

                if (reverse)
                {
                    while (item != nullptr && item->m_Next != nullptr)
                    {
                        item = (CGameItem *)item->m_Next;
                    }
                }

                CGUIHTMLGump *htmlGump = buyGump->m_ItemList[0];

                int currentY = 0;

                QFOR(shopItem, htmlGump->m_Items, CBaseGUI *)
                {
                    if (shopItem->Type == GOT_SHOPITEM)
                    {
                        currentY += shopItem->GetSize().Height;
                    }
                }

                while (item != nullptr)
                {
                    CGUIShopItem *shopItem = (CGUIShopItem *)htmlGump->Add(new CGUIShopItem(
                        item->Serial,
                        item->Graphic,
                        item->Color,
                        item->Count,
                        item->Price,
                        item->GetName(),
                        0,
                        currentY));
                    shopItem->NameFromCliloc = item->NameFromCliloc;

                    if (currentY == 0)
                    {
                        shopItem->Selected = true;
                        shopItem->CreateNameText();
                        shopItem->UpdateOffsets();
                    }

                    currentY += shopItem->GetSize().Height;

                    if (reverse)
                    {
                        item = (CGameItem *)item->m_Prev;
                    }
                    else
                    {
                        item = (CGameItem *)item->m_Next;
                    }
                }

                htmlGump->CalculateDataSize();
            }
        }
        else
        {
            LOG("Buy vendor not found!\n");
        }
    }
    else //Container
    {
        uint16_t graphic = 0xFFFF;

        for (int i = 0; i < (int)g_ContainerOffset.size(); i++)
        {
            if (gumpid == g_ContainerOffset[i].Gump)
            {
                graphic = (uint16_t)i;
                break;
            }
        }

        if (graphic == 0xFFFF)
        {
            return;
        }

        g_ContainerRect.Calculate(gumpid);

        gump = new CGumpContainer(serial, gumpid, g_ContainerRect.X, g_ContainerRect.Y);
        gump->Graphic = graphic;
        g_Orion.ExecuteGump(gumpid);
    }

    if (gump == nullptr)
    {
        return;
    }

    gump->ID = gumpid;

    if (gumpid != 0x0030)
    {
        if (static_cast<unsigned int>(!g_ContainerStack.empty()) != 0u)
        {
            for (deque<CContainerStackItem>::iterator cont = g_ContainerStack.begin();
                 cont != g_ContainerStack.end();
                 ++cont)
            {
                if (cont->Serial == serial)
                {
                    gump->SetX(cont->X);
                    gump->SetY(cont->Y);
                    gump->Minimized = cont->Minimized;
                    gump->MinimizedX = cont->MinimizedX;
                    gump->MinimizedY = cont->MinimizedY;
                    gump->LockMoving = cont->LockMoving;

                    if (gumpid != 0xFFFF)
                    {
                        if (cont->Minimized)
                        {
                            gump->Page = 1;
                        }
                        else
                        {
                            gump->Page = 2;
                        }
                    }

                    g_ContainerStack.erase(cont);

                    break;
                }
            }

            if (g_CheckContainerStackTimer < g_Ticks)
            {
                g_ContainerStack.clear();
            }
        }

        CGameItem *obj = g_World->FindWorldItem(serial);

        if (obj != nullptr)
        {
            /*if (gumpid != 0xFFFF)*/ obj->Opened = true;
            if (!obj->IsCorpse() && gumpid != 0xFFFF)
            {
                g_World->ClearContainer(obj);
            }

            if (gumpid == 0xFFFF)
            {
                ((CGumpSpellbook *)gump)->UpdateGraphic(obj->Graphic);
            }
        }
    }

    g_GumpManager.AddGump(gump);
}

PACKET_HANDLER(UpdateSkills)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint8_t type = ReadUInt8();
    bool haveCap = (((type != 0u) && type <= 0x03) || type == 0xDF);
    bool isSingleUpdate = (type == 0xFF || type == 0xDF);
    LOG("Skill update type %i (cap=%d)\n", type, haveCap);

    if (type == 0xFE)
    {
        int count = ReadInt16BE();
        g_SkillsManager.Clear();

        for (int i = 0; i < count; i++)
        {
            bool haveButton = (ReadUInt8() != 0);
            int nameLength = ReadUInt8();
            g_SkillsManager.Add(CSkill(haveButton, ReadString(nameLength)));
        }

        g_SkillsManager.Sort();

        return;
    }

    CGumpSkills *gump = (CGumpSkills *)g_GumpManager.UpdateGump(0, 0, GT_SKILLS);

    if (!isSingleUpdate && (type == 1 || type == 3 || g_SkillsManager.SkillsRequested))
    {
        g_SkillsManager.SkillsRequested = false;

        if (gump == nullptr)
        {
            gump = new CGumpSkills(0, 0, false, 250);
            g_GumpManager.AddGump(gump);
        }

        gump->Visible = true;
    }

    while (!IsEOF())
    {
        uint16_t id = ReadUInt16BE();

        if (IsEOF())
        {
            break;
        }

        if ((id == 0u) && (type == 0u))
        {
            break;
        }
        if ((type == 0u) || type == 0x02)
        {
            id--;
        }

        uint16_t baseVal = ReadUInt16BE();
        uint16_t realVal = ReadUInt16BE();
        uint8_t lock = ReadUInt8();
        uint16_t cap = 1000;

        if (haveCap)
        {
            cap = ReadUInt16BE();
        }

        CSkill *skill = g_SkillsManager.Get(id);

        if (id < g_SkillsManager.Count && skill != nullptr)
        {
            if (isSingleUpdate)
            {
                float change = (float)(baseVal / 10.0f) - skill->BaseValue;

                if (change != 0.0f)
                {
                    char str[128] = { 0 };
                    sprintf_s(
                        str,
                        "Your skill in %s has %s by %.1f%%.  It is now %.1f%%.",
                        skill->Name.c_str(),
                        ((change < 0) ? "decreased" : "increased"),
                        std::abs(change),
                        skill->BaseValue + change);
                    //else if (change > 0) sprintf(str, "Your skill in %s has increased by %.1f%%.  It is now %.1f%%.", UO->m_Skills[id].m_Name.c_str(), change, obj->GetSkillBaseValue(id) + change);
                    g_Orion.CreateTextMessage(TT_SYSTEM, 0, 3, 0x58, str);
                }
            }

            skill->BaseValue = (float)(baseVal / 10.0f);
            skill->Value = (float)(realVal / 10.0f);
            skill->Cap = (float)(cap / 10.0f);
            skill->Status = lock;

            if (gump != nullptr)
            {
                gump->UpdateSkillValue(id);
            }

            /*if (haveCap)
				LOG("Skill %i is %i|%i|%i\n", id, baseVal, realVal, cap);
			else
				LOG("Skill %i is %i|%i\n", id, baseVal, realVal);*/
        }
        //else
        //	LOG("Unknown skill update %d\n", id);

        if (isSingleUpdate)
        {
            break;
        }
    }

    g_SkillsManager.UpdateSkillsSum();

    if (gump != nullptr)
    {
        gump->UpdateSkillsSum();
    }

    LOG("Skill(s) updated!\n");
}

PACKET_HANDLER(ExtendedCommand)
{
    DEBUG_TRACE_FUNCTION;
    uint16_t cmd = ReadUInt16BE();

    switch (cmd)
    {
        case 0:
            break;
        case 1: //Initialize Fast Walk Prevention
        {
            for (int i = 0; i < 6; i++)
            {
                g_Player->m_FastWalkStack.SetValue((int)i, ReadUInt32BE());
            }

            break;
        }
        case 2: //Add key to Fast Walk Stack
        {
            g_Player->m_FastWalkStack.AddValue(ReadUInt32BE());

            break;
        }
        case 4: //Close generic gump
        {
            uint32_t id = ReadUInt32BE();
            uint32_t button = ReadUInt32BE();

            for (CGump *gump = (CGump *)g_GumpManager.m_Items; gump != nullptr;)
            {
                CGump *next = (CGump *)gump->m_Next;

                if (gump->GumpType == GT_GENERIC && gump->ID == id)
                {
                    ((CGumpGeneric *)gump)->SendGumpResponse(button);
                }

                gump = next;
            }

            break;
        }
        case 5: //Screen size
        {
            //g_GameWindowSizeX = unpack16(buf + 5);
            //g_GameWindowSizeY = unpack16(buf + 9);
            break;
        }
        case 6: //Party commands
        {
            g_Party.ParsePacketData(*this);

            break;
        }
        case 8: //Set cursor / map
        {
            g_Orion.ChangeMap(ReadUInt8());

            break;
        }
        case 0xC: //Close statusbar gump
        {
            uint32_t serial = ReadUInt32BE();
            g_GumpManager.CloseGump(serial, 0, GT_STATUSBAR);

            break;
        }
        case 0x10: //DisplayEquipmentInfo
        {
            uint32_t serial = ReadUInt32BE();
            CGameItem *item = g_World->FindWorldItem(serial);
            if (item == nullptr)
            {
                return;
            }

            item->JournalPrefix = "";
            wstring str = {};
            int clilocNum = ReadInt32BE();
            if (clilocNum != 0)
            {
                str = g_ClilocManager.Cliloc(g_Language)->GetW(clilocNum, true);
                if (str.length() > 0)
                {
                    item->SetName(ToString(str));
                }

                g_Orion.CreateUnicodeTextMessage(TT_OBJECT, serial, 0x03, 0x3B2, str);
            }

            str = {};
            uint16_t crafterNameLen = 0;
            uint32_t next = ReadUInt32BE();
            if (next == 0xFFFFFFFD)
            {
                crafterNameLen = ReadUInt16BE();
                if (crafterNameLen != 0u)
                {
                    str = L"Crafted by " + DecodeUTF8(ReadString(crafterNameLen));
                }
            }

            if (crafterNameLen != 0)
            {
                next = ReadUInt32BE();
            }
            if (next == 0xFFFFFFFC)
            {
                str += L"[Unidentified";
            }

            // -4 потому что последние 4 байта в пакете 0xFFFFFFFF
            uint8_t *end = Start + Size - 4;
            uint8_t count = 0;
            while (Ptr < end)
            {
                if (count != 0 || next == 0xFFFFFFFD || next == 0xFFFFFFFC)
                {
                    next = ReadInt32BE();
                }
                short charges = ReadInt16BE();
                wstring attrsString = g_ClilocManager.Cliloc(g_Language)->GetW(next, true);
                if (charges == -1)
                {
                    if (count > 0)
                    {
                        str += L"/";
                        str += attrsString;
                    }
                    else
                    {
                        str += L" [";
                        str += attrsString;
                    }
                }
                else
                {
                    str += L"\n[";
                    str += attrsString;
                    str += L" : ";
                    str += std::to_wstring(charges);
                    str += L"]";
                    count += 20;
                }
                count++;
            }
            if ((count < 20 && count > 0) || (next == 0xFFFFFFFC && count == 0))
            {
                str += L"]";
            }

            if (str.length() != 0u)
            {
                g_Orion.CreateUnicodeTextMessage(TT_OBJECT, serial, 0x03, 0x3B2, str);
            }
            CPacketMegaClilocRequestOld(serial).Send();
            break;
        }
        case 0x14: //Display Popup/context menu (2D and KR)
        {
            CGumpPopupMenu::Parse(*this);

            break;
        }
        case 0x16: //Close User Interface Windows
        {
            //ID:
            //0x01: Paperdoll
            //0x02: Status
            //0x08: Character Profile
            //0x0C: Container
            uint32_t id = ReadUInt32BE();
            uint32_t serial = ReadUInt32BE();

            switch (id)
            {
                case 1: //Paperdoll
                {
                    g_GumpManager.CloseGump(serial, 0, GT_PAPERDOLL);
                    break;
                }
                case 2: //Statusbar
                {
                    g_GumpManager.CloseGump(serial, 0, GT_STATUSBAR);
                    break;
                }
                case 8: //Character Profile
                {
                    g_GumpManager.CloseGump(serial, 0, GT_PROFILE);
                    break;
                }
                case 0xC: //Container
                {
                    g_GumpManager.CloseGump(serial, 0, GT_CONTAINER);
                    break;
                }
                default:
                    break;
            }

            break;
        }
        case 0x18: //Enable map (diff) patches
        {
            g_MapManager.ApplyPatches(*this);

            break;
        }
        case 0x19: //Extended stats
        {
            uint8_t version = ReadUInt8();
            uint32_t serial = ReadUInt32BE();

            switch (version)
            {
                case 0:
                {
                    CGameCharacter *bonded = g_World->FindWorldCharacter(serial);
                    if (bonded == nullptr)
                    {
                        break;
                    }

                    bool dead = ReadUInt8() != 0u;
                    bonded->SetDead(dead);
                    break;
                }
                case 2:
                {
                    if (serial == g_PlayerSerial)
                    {
                        uint8_t updateGump = ReadUInt8();
                        uint8_t state = ReadUInt8();
                        g_DrawStatLockers = true;

                        g_Player->LockStr = (state >> 4) & 3;
                        g_Player->LockDex = (state >> 2) & 3;
                        g_Player->LockInt = state & 3;

                        CGump *statusbar = g_GumpManager.GetGump(g_PlayerSerial, 0, GT_STATUSBAR);

                        if (statusbar != nullptr && !statusbar->Minimized)
                        {
                            statusbar->WantUpdateContent = true;
                        }
                    }
                    break;
                }
                case 5:
                {
                    if (g_World == nullptr)
                    {
                        return;
                    }

                    CGameCharacter *character = g_World->FindWorldCharacter(serial);
                    if (character == nullptr)
                    {
                        break;
                    }

                    if (Size == 19)
                    {
                        bool dead = ReadUInt8() != 0u;
                        character->SetDead(dead);
                    }
                    break;
                }
            }
            break;
        }
        case 0x1B: //New spellbook content
        {
            Move(2);
            uint32_t serial = ReadUInt32BE();

            CGameItem *spellbook = g_World->FindWorldItem(serial);

            if (spellbook == nullptr)
            {
                LOG("Where is a spellbook?!?\n");
                return;
            }

            g_World->ClearContainer(spellbook);

            uint16_t graphic = ReadUInt16BE();
            SPELLBOOK_TYPE bookType = (SPELLBOOK_TYPE)ReadUInt16BE();

            for (int j = 0; j < 2; j++)
            {
                uint32_t spells = 0;

                for (int i = 0; i < 4; i++)
                {
                    spells |= (ReadUInt8() << (i * 8));
                }

                for (int i = 0; i < 32; i++)
                {
                    if ((spells & (1 << i)) != 0u)
                    {
                        CGameItem *spellItem = new CGameItem();
                        spellItem->Graphic = 0x1F2E;
                        spellItem->Count = ((int)j * 32) + (int)i + 1;

                        spellbook->AddItem(spellItem);
                    }
                }
            }

            break;
        }
        case 0x1D:
        {
            //house revision state, server sends this when player comes in range of a custom house
            uint32_t serial = ReadUInt32BE();
            uint32_t revision = ReadUInt32BE();

            CCustomHouse *house = g_CustomHousesManager.Get(serial);
            LOG("Seek house: 0x%08X 0x%08X\n", serial, revision);

            if (house != nullptr)
            {
                LOG("House found: 0x%08X 0x%08X\n", house->Serial, house->Revision);
            }

            if (house == nullptr || house->Revision != revision)
            {
                CPacketCustomHouseDataReq(serial).Send();
            }
            else
            {
                house->Paste(g_World->FindWorldItem(serial));
            }

            break;
        }
        case 0x20:
        {
            uint32_t serial = ReadUInt32BE();
            uint8_t type = ReadUInt8();
            uint16_t graphic = ReadUInt16BE();
            uint16_t x = ReadUInt16BE();
            uint16_t y = ReadUInt16BE();
            uint8_t z = ReadUInt8();

            switch (type)
            {
                case CHUT_UPDATE:
                {
                    break;
                }
                case CHUT_REMOVE:
                {
                    break;
                }
                case CHUT_UPDATE_MULTI_POS:
                {
                    break;
                }
                case CHUT_CONSTRUCT_BEGIN:
                {
                    if (g_GumpManager.GetGump(0, 0, GT_CUSTOM_HOUSE) != nullptr)
                    {
                        break;
                    }

                    CGumpCustomHouse *gump = new CGumpCustomHouse(serial, 50, 50);

                    g_GumpManager.AddGump(gump);

                    break;
                }
                case CHUT_CONSTRUCT_END:
                {
                    g_GumpManager.CloseGump(serial, 0, GT_CUSTOM_HOUSE);
                    break;
                }
                default:
                    break;
            }

            break;
        }
        case 0x21:
        {
            for (int i = 0; i < 2; i++)
            {
                g_Ability[i] &= 0x7F;
                g_GumpManager.UpdateContent((int)i, 0, GT_ABILITY);
            }

            break;
        }
        case 0x22:
        {
            if (g_World == nullptr)
            {
                return;
            }

            Move(1);
            uint32_t serial = ReadUInt32BE();
            CGameCharacter *character = g_World->FindWorldCharacter(serial);

            if (character != nullptr)
            {
                int damage = ReadUInt8();

                CTextData *text = new CTextData();
                text->Unicode = false;
                text->Font = 3;
                text->Serial = serial;
                text->Color = (serial == g_PlayerSerial ? 0x0034 : 0x0021);
                text->Type = TT_OBJECT;
                text->Text = std::to_string(damage);
                text->GenerateTexture(0);
                text->SetX(text->m_Texture.Width / 2);
                int height = text->m_Texture.Height;

                CTextData *head = (CTextData *)character->m_DamageTextControl.Last();

                if (head != nullptr)
                {
                    height += head->GetY();

                    if (height > 0)
                    {
                        if (height > 100)
                        {
                            height = 0;
                        }

                        text->SetY(height);
                    }
                }

                character->m_DamageTextControl.Add(text);
                text->Timer = g_Ticks + DAMAGE_TEXT_NORMAL_DELAY;
            }

            break;
        }
        case 0x26:
        {
            uint8_t val = ReadUInt8();

            if (val > CST_FAST_UNMOUNT_AND_CANT_RUN)
            {
                val = 0;
            }

            g_SpeedMode = (CHARACTER_SPEED_TYPE)val;

            break;
        }
        default:
            break;
    }
}

PACKET_HANDLER(DenyWalk)
{
    DEBUG_TRACE_FUNCTION;
    if (g_Player == nullptr)
    {
        return;
    }

    g_Ping = 0;

    uint8_t sequence = ReadUInt8();
    uint16_t x = ReadUInt16BE();
    uint16_t y = ReadUInt16BE();
    uint8_t direction = ReadUInt8();
    char z = ReadUInt8();

    g_Walker.DenyWalk(sequence, x, y, z);

    g_Player->Direction = direction;

    g_World->MoveToTop(g_Player);
}

PACKET_HANDLER(ConfirmWalk)
{
    DEBUG_TRACE_FUNCTION;
    if (g_Player == nullptr)
    {
        return;
    }

    uint8_t sequence = ReadUInt8();

    //player->SetDirection(newdir);

    uint8_t newnoto = ReadUInt8() & (~0x40);

    if ((newnoto == 0u) || newnoto >= 8)
    {
        newnoto = 0x01;
    }

    g_Player->Notoriety = newnoto;

    g_Walker.ConfirmWalk(sequence);

    g_World->MoveToTop(g_Player);
}

PACKET_HANDLER(OpenUrl)
{
    DEBUG_TRACE_FUNCTION;
    g_Orion.GoToWebLink(ReadString());
}

PACKET_HANDLER(Target)
{
    DEBUG_TRACE_FUNCTION;
    g_Target.SetData(*this);

    if (g_PartyHelperTimer > g_Ticks && (g_PartyHelperTarget != 0u))
    {
        g_Target.SendTargetObject(g_PartyHelperTarget);
        g_PartyHelperTimer = 0;
        g_PartyHelperTarget = 0;
    }
}

PACKET_HANDLER(Talk)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        if (g_GameState == GS_GAME_CONNECT)
        {
            uint32_t serial = ReadUInt32BE();
            uint16_t graphic = ReadUInt16BE();
            SPEECH_TYPE type = (SPEECH_TYPE)ReadUInt8();
            uint16_t textColor = ReadUInt16BE();
            uint16_t font = ReadUInt16BE();

            string name(ReadString());
            if (Size > 44)
            {
                Ptr = Start + 44;
                g_ConnectionScreen.SetConnectionFailed(true);
                g_ConnectionScreen.SetTextA(ReadString());
            }
        }

        return;
    }

    uint32_t serial = ReadUInt32BE();
    uint16_t graphic = ReadUInt16BE();
    SPEECH_TYPE type = (SPEECH_TYPE)ReadUInt8();
    uint16_t textColor = ReadUInt16BE();
    uint16_t font = ReadUInt16BE();
    string name(ReadString());

    if ((serial == 0u) && (graphic == 0u) && type == ST_NORMAL && font == 0xFFFF &&
        textColor == 0xFFFF && ToLowerA(name) == "system")
    {
        uint8_t sbuffer[0x28] = { 0x03, 0x00, 0x28, 0x20, 0x00, 0x34, 0x00, 0x03, 0xdb, 0x13,
                                  0x14, 0x3f, 0x45, 0x2c, 0x58, 0x0f, 0x5d, 0x44, 0x2e, 0x50,
                                  0x11, 0xdf, 0x75, 0x5c, 0xe0, 0x3e, 0x71, 0x4f, 0x31, 0x34,
                                  0x05, 0x4e, 0x18, 0x1e, 0x72, 0x0f, 0x59, 0xad, 0xf5, 0x00 };

        g_Orion.Send(sbuffer, 0x28);

        return;
    }

    if (font >= g_FontManager.FontCount)
    {
        return;
    }

    string str{};
    if (Size > 44)
    {
        Ptr = Start + 44;
        str = ReadString();
    }

    LOG("%s: %s\n", name.c_str(), str.c_str());

    CGameObject *obj = g_World->FindWorldObject(serial);

    if (type == ST_BROADCAST || /*type == ST_SYSTEM ||*/ serial == 0xFFFFFFFF || (serial == 0u) ||
        (ToLowerA(name) == "system" && obj == nullptr))
    {
        g_Orion.CreateTextMessage(TT_SYSTEM, serial, (uint8_t)font, textColor, str);
    }
    else
    {
        if (type == ST_EMOTE)
        {
            textColor = g_ConfigManager.EmoteColor;
            str = "*" + str + "*";
        }

        if (obj != nullptr)
        {
            //reset
            obj->JournalPrefix = "";
            if (obj->GetName().length() == 0u)
            {
                obj->SetName(name);

                if (obj->NPC)
                {
                    g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);
                }
            }

            if (type == ST_SYSTEM)
            {
                obj->JournalPrefix = "You see: ";
            }
            else if (obj->GetName().length() != 0u)
            {
                obj->JournalPrefix = obj->GetName() + ": ";
            }
        }

        g_Orion.CreateTextMessage(TT_OBJECT, serial, (uint8_t)font, textColor, str);
    }
}

PACKET_HANDLER(UnicodeTalk)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        if (g_GameState == GS_GAME_CONNECT)
        {
            uint32_t serial = ReadUInt32BE();
            uint16_t graphic = ReadUInt16BE();
            SPEECH_TYPE type = (SPEECH_TYPE)ReadUInt8();
            uint16_t textColor = ReadUInt16BE();
            uint16_t font = ReadUInt16BE();
            uint32_t language = ReadUInt32BE();

            string name(ReadString());

            if (Size > 48)
            {
                Ptr = Start + 48;
                g_ConnectionScreen.SetConnectionFailed(true);
                g_ConnectionScreen.SetTextA(ToString(ReadWStringBE((Size - 48) / 2)));
            }
        }

        return;
    }

    uint32_t serial = ReadUInt32BE();
    uint16_t graphic = ReadUInt16BE();
    SPEECH_TYPE type = (SPEECH_TYPE)ReadUInt8();
    uint16_t textColor = ReadUInt16BE();
    uint16_t font = ReadUInt16BE();
    uint32_t language = ReadUInt32BE();
    string name(ReadString());

    if ((serial == 0u) && (graphic == 0u) && type == ST_NORMAL && font == 0xFFFF &&
        textColor == 0xFFFF && ToLowerA(name) == "system")
    {
        uint8_t sbuffer[0x28] = { 0x03, 0x00, 0x28, 0x20, 0x00, 0x34, 0x00, 0x03, 0xdb, 0x13,
                                  0x14, 0x3f, 0x45, 0x2c, 0x58, 0x0f, 0x5d, 0x44, 0x2e, 0x50,
                                  0x11, 0xdf, 0x75, 0x5c, 0xe0, 0x3e, 0x71, 0x4f, 0x31, 0x34,
                                  0x05, 0x4e, 0x18, 0x1e, 0x72, 0x0f, 0x59, 0xad, 0xf5, 0x00 };

        g_Orion.Send(sbuffer, 0x28);

        return;
    }

    wstring str = {};
    if (Size > 48)
    {
        Ptr = Start + 48;
        str = ReadWStringBE((Size - 48) / 2);
    }

    LOG("%s: %s\n", name.c_str(), ToString(str).c_str());

    CGameObject *obj = g_World->FindWorldObject(serial);
    if (type == ST_GUILD_CHAT)
    {
        type = ST_BROADCAST;
        textColor = g_ConfigManager.GuildMessageColor;
        str = L"[Guild][" + ToWString(name) + L"]: " + str;
    }
    else if (type == ST_ALLIANCE_CHAT)
    {
        type = ST_BROADCAST;
        textColor = g_ConfigManager.AllianceMessageColor;
        str = L"[Alliance][" + ToWString(name) + L"]: " + str;
    }

    if (type == ST_BROADCAST /*|| type == ST_SYSTEM*/ || serial == 0xFFFFFFFF || (serial == 0u) ||
        (ToLowerA(name) == "system" && obj == nullptr))
    {
        g_Orion.CreateUnicodeTextMessage(
            TT_SYSTEM, serial, (uint8_t)g_ConfigManager.SpeechFont, textColor, str);
    }
    else
    {
        if (type == ST_EMOTE && (textColor == 0u))
        {
            textColor = g_ConfigManager.EmoteColor;
        }

        if (obj != nullptr)
        {
            //reset
            obj->JournalPrefix = "";

            if (obj->GetName().length() == 0u)
            {
                obj->SetName(name);

                if (obj->NPC)
                {
                    g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);
                }
            }

            if (type == ST_SYSTEM)
            {
                obj->JournalPrefix = "You see: ";
            }
            else if (obj->GetName().length() != 0u)
            {
                obj->JournalPrefix = obj->GetName() + ": ";
            }
        }

        g_Orion.CreateUnicodeTextMessage(
            TT_OBJECT, serial, (uint8_t)g_ConfigManager.SpeechFont, textColor, str);
    }
}

PACKET_HANDLER(ClientTalk)
{
    DEBUG_TRACE_FUNCTION;
    if (!g_AbyssPacket03First)
    {
        bool parse = true;

        if (Start[4] == 0x78)
        {
            Size = ReadInt16BE(2);
            Start += 4;
        }
        else if (Start[4] == 0x3C)
        {
            Size = ReadInt16BE(2);
            Start += 4;
        }
        else if (Start[4] == 0x25)
        {
            Size = 0x14;
            Start += 4;
        }
        else if (Start[4] == 0x2E)
        {
            Size = 0x0F;
            Start += 4;
        }
        else
        {
            parse = false;
        }

        if (parse)
        {
            OnPacket();
        }
    }

    g_AbyssPacket03First = false;
}

PACKET_HANDLER(MultiPlacement)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    //uint32_t serial = unpack32(buf + 2);
    //uint16_t graphic = unpack16(buf + 18);

    g_Target.SetMultiData(*this);
}

PACKET_HANDLER(GraphicEffect)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint8_t type = ReadUInt8();

    if (type > 3)
    {
        if (type == 4 && *Start == 0x70)
        {
            Move(8);
            uint16_t val = ReadInt16BE();

            if (val > SET_TO_BLACK_VERY_FAST)
            {
                val = SET_TO_BLACK_VERY_FAST;
            }

            g_ScreenEffectManager.Use(SEM_SUNSET, (SCREEN_EFFECT_TYPE)val, true);
        }

        return;
    }

    uint32_t sourceSerial = ReadUInt32BE();
    uint32_t destSerial = ReadUInt32BE();
    uint16_t graphic = ReadUInt16BE();
    short sourceX = ReadInt16BE();
    short sourceY = ReadInt16BE();
    char sourceZ = ReadInt8();
    short destX = ReadInt16BE();
    short destY = ReadInt16BE();
    char destZ = ReadInt8();
    uint8_t speed = ReadUInt8();
    short duration = (short)ReadUInt8() * 50;
    //what is in 24-25 bytes?
    Move(2);
    uint8_t fixedDirection = ReadUInt8();
    uint8_t explode = ReadUInt8();

    uint32_t color = 0;
    uint32_t renderMode = 0;

    if (*Start != 0x70)
    {
        //0xC0
        color = ReadUInt32BE();
        renderMode = ReadUInt32BE() % 7;

        if (*Start == 0xC7)
        {
            /*
			0000: c7 03 00 13 82 2f 00 00 00 00 37 6a 05 d6 06 47 : ...../....7j...G
			0010: 15 05 d6 06 47 15 09 20 00 00 01 00 00 00 00 00 : ....G.. ........
			0020: 00 00 00 00 13 8d 00 01 00 00 00 13 82 2f 03 00 : ............./..
			0030: 00 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- : .
			*/
        }
    }

    CGameEffect *effect = nullptr;
    if (type == 0u) //Moving
    {
        effect = new CGameEffectMoving();

        if (speed == 0u)
        {
            speed++;
        }

        ((CGameEffectMoving *)effect)->MoveDelay = 20 / speed;
    }
    else
    {
        effect = new CGameEffect();
    }

    effect->EffectType = (EFFECT_TYPE)type;
    effect->Serial = sourceSerial;
    effect->DestSerial = destSerial;
    effect->Graphic = graphic;

    CGameCharacter *sourceObject = g_World->FindWorldCharacter(sourceSerial);

    if (sourceObject != nullptr)
    {
        effect->SetX(sourceObject->GetX());
        effect->SetY(sourceObject->GetY());
        effect->SetZ(sourceObject->GetZ());
    }
    else
    {
        effect->SetX(sourceX);
        effect->SetY(sourceY);
        effect->SetZ(sourceZ);
    }

    CGameCharacter *destObject = g_World->FindWorldCharacter(destSerial);

    if (destObject != nullptr)
    {
        effect->DestX = destObject->GetX();
        effect->DestY = destObject->GetY();
        effect->DestZ = destObject->GetZ();
    }
    else
    {
        effect->DestX = destX;
        effect->DestY = destY;
        effect->DestZ = destZ;
    }

    uintptr_t addressAnimData = (uintptr_t)g_FileManager.m_AnimdataMul.Start;

    if (addressAnimData != 0u)
    {
        ANIM_DATA *pad =
            (ANIM_DATA *)(addressAnimData + ((graphic * 68) + 4 * ((graphic / 8) + 1)));

        effect->Speed = pad->FrameInterval * 45;
        //effect->Speed = (pad->FrameInterval - effect->Speed) * 45;
    }
    else
    {
        effect->Speed = speed + 6;
    }

    effect->Duration = g_Ticks + duration;
    effect->FixedDirection = (fixedDirection != 0);
    effect->Explode = (explode != 0);

    effect->Color = (uint16_t)color;
    effect->RenderMode = renderMode;

    g_EffectManager.AddEffect(effect);
}

PACKET_HANDLER(DeathScreen)
{
    DEBUG_TRACE_FUNCTION;
    uint8_t action = ReadUInt8();

    if (action != 1)
    {
        g_GameScreen.SmoothScreenAction = 0;
        g_ScreenEffectManager.Use(SEM_SUNSET, SET_TO_BLACK, true);

        g_Weather.Reset();
        g_Target.Reset();

        g_GumpManager.CloseGump(0, 0, GT_DRAG);

        if (g_ConfigManager.GetMusic())
        {
            g_Orion.PlayMusic(42, true);
        }

        g_Orion.ChangeWarmode(0);

        g_DeathScreenTimer = g_Ticks + DEATH_SCREEN_DELAY;
    }
}

PACKET_HANDLER(PlaySoundEffect)
{
    DEBUG_TRACE_FUNCTION;
    Move(1);
    uint16_t index = ReadUInt16BE();
    uint16_t volume = ReadUInt16BE();
    uint16_t xCoord = ReadUInt16BE(); // CHECK: unsigned for position?
    uint16_t yCoord = ReadUInt16BE();
    g_Orion.PlaySoundEffectAtPosition(index, xCoord, yCoord);
}

PACKET_HANDLER(PlayMusic)
{
    DEBUG_TRACE_FUNCTION;
    uint16_t index = ReadUInt16BE();

    //LOG("Play midi music 0x%04X\n", index);
    if (!g_ConfigManager.GetMusic() || !g_OrionWindow.IsActive() ||
        g_ConfigManager.GetMusicVolume() < 1)
    {
        return;
    }

    g_Orion.PlayMusic(index);
}

PACKET_HANDLER(DragAnimation)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint16_t graphic = ReadUInt16BE();
    graphic += ReadUInt8(); //graphic increment

    uint16_t color = ReadUInt16BE();
    uint16_t count = ReadUInt16BE();

    uint32_t sourceSerial = ReadUInt32BE();
    short sourceX = ReadInt16BE();
    short sourceY = ReadInt16BE();
    char sourceZ = ReadInt8();
    uint32_t destSerial = ReadUInt32BE();
    short destX = ReadInt16BE();
    short destY = ReadInt16BE();
    char destZ = ReadInt8();

    if (graphic == 0x0EED)
    {
        graphic = 0x0EEF;
    }
    else if (graphic == 0x0EEA)
    {
        graphic = 0x0EEC;
    }
    else if (graphic == 0x0EF0)
    {
        graphic = 0x0EF2;
    }

    CGameCharacter *sourceObj = g_World->FindWorldCharacter(sourceSerial);

    if (sourceObj == nullptr)
    {
        sourceSerial = 0;
    }
    else
    {
        sourceX = sourceObj->GetX();
        sourceY = sourceObj->GetY();
        sourceZ = sourceObj->GetZ();
    }

    CGameCharacter *destObj = g_World->FindWorldCharacter(destSerial);

    if (destObj == nullptr)
    {
        destSerial = 0;
    }
    else
    {
        destX = destObj->GetX();
        destY = destObj->GetY();
        destZ = destObj->GetZ();
    }

    CGameEffect *effect = nullptr;

    uint8_t speed = 5;

    if ((sourceSerial == 0u) || (destSerial == 0u)) //Игрок/НПС кладет предмет в контейнер
    {
        effect = new CGameEffectMoving();
        effect->EffectType = EF_MOVING;
        effect->FixedDirection = true;

        ((CGameEffectMoving *)effect)->MoveDelay = 20 / speed;
    }
    else //Предмет взяли из контейнера
    {
        effect = new CGameEffectDrag();
        effect->EffectType = EF_DRAG;
    }

    effect->Graphic = graphic;
    effect->Color = color;
    effect->Serial = sourceSerial;
    effect->DestSerial = destSerial;
    effect->SetX(sourceX);
    effect->SetY(sourceY);
    effect->SetZ(sourceZ);
    effect->DestX = destX;
    effect->DestY = destY;
    effect->DestZ = destZ;
    effect->Duration = g_Ticks + 5000;

    uintptr_t addressAnimData = (uintptr_t)g_FileManager.m_AnimdataMul.Start;

    if (addressAnimData != 0u)
    {
        ANIM_DATA *pad =
            (ANIM_DATA *)(addressAnimData + ((graphic * 68) + 4 * ((graphic / 8) + 1)));

        effect->Speed = pad->FrameInterval * 45;
    }
    else
    {
        effect->Speed = speed + 6;
    }

    g_EffectManager.AddEffect(effect);
}

PACKET_HANDLER(CorpseEquipment)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t cserial = ReadUInt32BE();

    uint8_t *end = Start + Size;

    int layer = ReadUInt8();

    while ((layer != 0) && Ptr < end)
    {
        uint32_t serial = ReadUInt32BE();

        CGameItem *obj = g_World->FindWorldItem(serial);

        if (obj != nullptr && obj->Container == cserial)
        {
            g_World->PutEquipment(obj, cserial, layer);
        }

        layer = ReadUInt8();
    }
}

PACKET_HANDLER(ASCIIPrompt)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    g_ConsolePrompt = PT_ASCII;
    memcpy(&g_LastASCIIPrompt[0], &Start[0], 11);
}

PACKET_HANDLER(UnicodePrompt)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    g_ConsolePrompt = PT_UNICODE;
    memcpy(&g_LastUnicodePrompt[0], &Start[0], 11);
}

PACKET_HANDLER(CharacterAnimation)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    if (obj != nullptr)
    {
        uint16_t action = ReadUInt16BE();
        uint16_t frameCount = ReadUInt16BE();
        frameCount = 0;
        uint16_t repeatMode = ReadUInt16BE();
        bool frameDirection = (ReadUInt8() == 0); //true - forward, false - backward
        bool repeat = (ReadUInt8() != 0);
        uint8_t delay = ReadUInt8();
        obj->SetAnimation(
            g_AnimationManager.GetReplacedObjectAnimation(obj, action),
            delay,
            (uint8_t)frameCount,
            (uint8_t)repeatMode,
            repeat,
            frameDirection);
        obj->AnimationFromServer = true;
    }
}

PACKET_HANDLER(NewCharacterAnimation)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    if (obj != nullptr)
    {
        uint16_t type = ReadUInt16BE();
        uint16_t action = ReadUInt16BE();
        uint8_t mode = ReadUInt8();

        uint8_t group = g_AnimationManager.GetObjectNewAnimation(obj, type, action, mode);

        obj->SetAnimation(group);

        obj->AnimationRepeatMode = 1;
        obj->AnimationDirection = true;

        if ((type == 1 || type == 2) && obj->Graphic == 0x0015)
        {
            obj->AnimationRepeat = true;
        }

        obj->AnimationFromServer = true;
    }
}

PACKET_HANDLER(DisplayQuestArrow)
{
    DEBUG_TRACE_FUNCTION;
    g_QuestArrow.Timer = g_Ticks + 1000;
    g_QuestArrow.Enabled = (ReadUInt8() != 0);
    g_QuestArrow.X = ReadUInt16BE();
    g_QuestArrow.Y = ReadUInt16BE();
}

PACKET_HANDLER(ClientViewRange)
{
    DEBUG_TRACE_FUNCTION;
    g_ConfigManager.UpdateRange = ReadUInt8();
}

PACKET_HANDLER(KrriosClientSpecial)
{
    DEBUG_TRACE_FUNCTION;
    uint8_t type = ReadUInt8();
    if (type == 0xFE)
    {
        CPacketRazorAnswer().Send();
    }
}

PACKET_HANDLER(AssistVersion)
{
    DEBUG_TRACE_FUNCTION;
    uint32_t version = ReadUInt32BE();
    CPacketAssistVersion(version, g_Config.ClientVersionString).Send();
}

PACKET_HANDLER(CharacterListNotification)
{
    DEBUG_TRACE_FUNCTION;
    g_Orion.InitScreen(GS_DELETE);
    g_ConnectionScreen.SetType(CST_CHARACTER_LIST);
    g_ConnectionScreen.SetConnectionFailed(true);
    g_ConnectionScreen.SetErrorCode(ReadUInt8());
}

PACKET_HANDLER(ErrorCode)
{
    DEBUG_TRACE_FUNCTION;
    uint8_t code = ReadUInt8();

    g_Orion.InitScreen(GS_DELETE);
    g_ConnectionScreen.SetType(CST_GAME_LOGIN);
    g_ConnectionScreen.SetErrorCode(code);

    if (code > 7)
    {
        g_ConnectionScreen.SetErrorCode(3);
    }
}

PACKET_HANDLER(AttackCharacter)
{
    DEBUG_TRACE_FUNCTION;
    g_LastAttackObject = ReadUInt32BE();

    if (g_LastAttackObject != 0 && g_World != nullptr)
    {
        CGameCharacter *obj = g_World->FindWorldCharacter(g_LastAttackObject);

        if (obj != nullptr && (obj->MaxHits == 0))
        {
            CPacketStatusRequest(g_LastAttackObject).Send();
        }
    }
}

PACKET_HANDLER(Season)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint8_t season = ReadUInt8();

    if (season > ST_DESOLATION)
    {
        season = 0;
    }

    if ((g_Player->Dead() /*|| g_Season == ST_DESOLATION*/) && (SEASON_TYPE)season != ST_DESOLATION)
    {
        return;
    }

    g_OldSeason = (SEASON_TYPE)season;
    g_OldSeasonMusic = ReadUInt8();

    if (g_Season == ST_DESOLATION)
    {
        g_OldSeasonMusic = DEATH_MUSIC_INDEX;
    }

    g_Orion.ChangeSeason((SEASON_TYPE)season, g_OldSeasonMusic);
}

PACKET_HANDLER(DisplayDeath)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    uint32_t corpseSerial = ReadUInt32BE();
    uint32_t running = ReadUInt32BE();

    CGameCharacter *owner = g_World->FindWorldCharacter(serial);

    if (owner == nullptr)
    {
        return;
    }

    serial |= 0x80000000;

    g_World->ReplaceObject(owner, serial);

    if (corpseSerial != 0u)
    {
        g_CorpseManager.Add(CCorpse(corpseSerial, serial, owner->Direction, running != 0));
    }

    uint8_t group = g_AnimationManager.GetDieGroupIndex(owner->Graphic, running != 0);

    owner->SetAnimation(group, 0, 5, 1, false, false);
}

PACKET_HANDLER(OpenChat)
{
    DEBUG_TRACE_FUNCTION;
    uint8_t newbuf[4] = { 0xf0, 0x00, 0x04, 0xff };
    g_Orion.Send(newbuf, 4);
}

PACKET_HANDLER(DisplayClilocString)
{
    DEBUG_TRACE_FUNCTION;

    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    uint16_t graphic = ReadUInt16BE();
    uint8_t type = ReadUInt8();
    uint16_t color = ReadUInt16BE();
    uint8_t font = (uint8_t)ReadUInt16BE();
    uint32_t cliloc = ReadUInt32BE();
    if (!g_FontManager.UnicodeFontExists(font))
    {
        font = 0;
    }

    uint8_t flags = 0;
    if (*Start == 0xCC)
    {
        flags = ReadUInt8();
    }

    string name = ReadString(30);
    wstring affix{};
    if (*Start == 0xCC)
    {
        affix = DecodeUTF8(ReadString());
    }

    wstring wc = ReadWStringLE();
    auto message = g_ClilocManager.ParseArgumentsToClilocString(cliloc, false, wc);
    message += affix;

    CGameObject *obj = g_World->FindWorldObject(serial);
    if (/*type == ST_BROADCAST || type == ST_SYSTEM ||*/ serial == 0xFFFFFFFF || (serial == 0u) ||
        (ToLowerA(name) == "system" && obj == nullptr))
    {
        g_Orion.CreateUnicodeTextMessage(TT_SYSTEM, serial, font, color, message);
    }
    else
    {
        /*if (type == ST_EMOTE)
		{
			color = ConfigManager.EmoteColor;
			str = L"*" + str + L"*";
		}*/
        if (obj != nullptr)
        {
            if (name.length() == 0u)
            {
                obj->JournalPrefix = "You see: ";
            }
            else
            {
                obj->JournalPrefix = name + ": ";
                obj->SetName(name);
                if (obj->NPC)
                {
                    g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);
                }
            }
        }
        g_Orion.CreateUnicodeTextMessage(TT_OBJECT, serial, font, color, message);
    }
}

PACKET_HANDLER(MegaCliloc)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint16_t unknown = ReadUInt16BE();
    if (unknown > 1)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    Move(2);
    uint32_t clilocRevision = ReadUInt32BE();

    uint8_t *end = Start + Size;
    vector<wstring> list;
    while (Ptr < end)
    {
        uint32_t cliloc = ReadUInt32BE();
        if (cliloc == 0u)
        {
            break;
        }

        const int len = ReadInt16BE();
        wstring argument = {};
        if (len > 0)
        {
            argument = ReadWStringLE(len / 2);
        }

        wstring str = g_ClilocManager.ParseArgumentsToClilocString(cliloc, true, argument);
        //LOG("Cliloc: argstr=%s\n", ToString(str).c_str());
        //LOG("Cliloc: 0x%08X len=%i arg=%s\n", cliloc, len, ToString(argument).c_str());

        bool canAdd = true;
        for (const wstring &tempStr : list)
        {
            if (tempStr == str)
            {
                canAdd = false;
                break;
            }
        }

        if (canAdd)
        {
            list.push_back(str);
        }
    }

    CGameItem *container = nullptr;
    CGameObject *obj = g_World->FindWorldObject(serial);
    if (obj != nullptr)
    {
        container = g_World->FindWorldItem(obj->Container);
    }

    bool inBuyList = false;
    if (container != nullptr)
    {
        inBuyList =
            (container->Layer == OL_BUY || container->Layer == OL_BUY_RESTOCK ||
             container->Layer == OL_SELL);
    }

    bool first = true;
    wstring name = {};
    wstring data = {};
    if (!list.empty())
    {
        for (const wstring &str : list)
        {
            if (first)
            {
                name = str;
                if (obj != nullptr && !obj->NPC)
                {
                    obj->SetName(ToString(str));
                    obj->GenerateObjectHandlesTexture(str);
                }

                first = false;
            }
            else
            {
                if (data.length() != 0u)
                {
                    data += L"\n";
                }

                data += str;
            }
        }
    }

    //LOG_DUMP((uint8_t *)message.c_str(), message.length() * 2);
    g_ObjectPropertiesManager.Add(serial, CObjectProperty(serial, clilocRevision, name, data));
    if (obj != nullptr && g_ToolTip.m_Object == obj)
    {
        g_ObjectPropertiesManager.Reset();
    }

    //LOG("message=%s\n", ToString(message).c_str());
    if (inBuyList && (container->Serial != 0u))
    {
        CGumpShop *gump = (CGumpShop *)g_GumpManager.GetGump(container->Serial, 0, GT_SHOP);
        if (gump != nullptr)
        {
            CGUIHTMLGump *htmlGump = gump->m_ItemList[0];
            QFOR(shopItem, htmlGump->m_Items, CBaseGUI *)
            {
                if (shopItem->Type == GOT_SHOPITEM && shopItem->Serial == serial &&
                    ((CGUIShopItem *)shopItem)->NameFromCliloc)
                {
                    CGUIShopItem *si = (CGUIShopItem *)shopItem;
                    int oldHeight = si->GetSize().Height;

                    si->Name = ToString(name);
                    si->CreateNameText();
                    si->UpdateOffsets();

                    int delta = si->GetSize().Height - oldHeight;
                    if ((delta != 0) && shopItem->m_Next != nullptr)
                    {
                        QFOR(shopItem2, shopItem->m_Next, CBaseGUI *)
                        {
                            if (shopItem2->Type == GOT_SHOPITEM)
                            {
                                ((CGUIShopItem *)shopItem2)
                                    ->SetY(((CGUIShopItem *)shopItem2)->GetY() + delta);
                            }
                        }
                    }

                    break;
                }
            }

            htmlGump->CalculateDataSize();
        }
    }
}

PACKET_HANDLER(Damage)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    CGameCharacter *character = g_World->FindWorldCharacter(serial);

    if (character != nullptr)
    {
        int damage = ReadUInt16BE();

        CTextData *text = new CTextData();
        text->Unicode = false;
        text->Font = 3;
        text->Serial = serial;
        text->Color = (serial == g_PlayerSerial ? 0x0034 : 0x0021);
        text->Type = TT_OBJECT;
        text->Text = std::to_string(damage);
        text->GenerateTexture(0);
        text->SetX(text->m_Texture.Width / 2);
        int height = text->m_Texture.Height;

        CTextData *head = (CTextData *)character->m_DamageTextControl.Last();

        if (head != nullptr)
        {
            height += head->GetY();

            if (height > 0)
            {
                if (height > 100)
                {
                    height = 0;
                }

                text->SetY(height);
            }
        }

        character->m_DamageTextControl.Add(text);
        text->Timer = g_Ticks + DAMAGE_TEXT_NORMAL_DELAY;
    }
}

PACKET_HANDLER(BuffDebuff)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    /*
	df
	00 2e
	00 00 2b b5
	04 04
	00 01
	00 00 00 00 04 04 00 01 00 00 00 00
	01 ca
	00 00 00
	00 10 6a 6b
	00 10 6a 6c
	00 00 00 00
	00 00
	00 00
	00 00



	df
	00 46
	00 00 2b b5
	04 05
	00 01
	00 00 00 00 04 05 00 01 00 00 00 00
	00 85
	00 00 00
	00 10 6a 66
	00 10 56 82
	00 00 00 00
	00 01
	00 00
	09 00 2b 00 20 00 39 00 20 00 41 00 72 00 6d 00 6f 00 72 00 00 00
	00 01
	00 00




	Buffs And Attributes Packet.
	from server
	byte    ID (DF)
	word    Packet Size
	dword    Player Serial
	word    Buff Type
	(
	BonusStr = 0x01, BonusDex = 0x02, BonusInt = 0x03, BonusHits = 0x07, BonusStamina = 0x08, BonusMana = 0x09,
	RegenHits = 0x0A, RegenStam = 0x0B, RegenMana = 0x0C, NightSight = 0x0D, Luck = 0x0E, ReflectPhysical = 0x10,
	EnhancePotions = 0x11, AttackChance = 0x12, DefendChance = 0x13, SpellDamage = 0x14, CastRecovery = 0x15,
	CastSpeed = 0x16, ManaCost = 0x17, ReagentCost = 0x18, WeaponSpeed = 0x19, WeaponDamage = 0x1A,
	PhysicalResistance = 0x1B, FireResistance = 0x1C, ColdResistance = 0x1D, PoisonResistance = 0x1E,
	EnergyResistance = 0x1F, MaxPhysicalResistance = 0x20, MaxFireResistance = 0x21, MaxColdResistance = 0x22,
	MaxPoisonResistance = 0x23, MaxEnergyResistance = 0x24, AmmoCost = 0x26, KarmaLoss = 0x28, 0x3EA+ = buff icons
	)

	word    Buffs Count

	loop    Buffs >>>
	word    Source Type
	(
	0 = Character, 50 = two-handed weapon, 53 = one-handed weapon or spellbook, 54 = shield or ranged weapon,
	55 = shoes, 56 = pants or legs, 58 = helm or hat, 59 = gloves, 60 = ring, 61 = talisman, 62 = necklace or gorget,
	64 = waist, 65 = inner torso, 66 = bracelet, 69 = middle torso, 70 = earring, 71 = arms, 72 = cloak or quiver,
	74 = outer torso, 1000 = spells
	)

	word    0x00
	word    Buff Icon ID (0 for attributes)
	word    Buff Queue Index (Delta Value for attributes)
	dword    0x00
	word    Buff Duration in seconds (0 for attributes)
	byte[3]    0x00
	dword    Buff Title Cliloc
	dword    Buff Secondary Cliloc (0 for attributes)
	dword    Buff Third Cliloc (0 for attributes)
	word    Primary Cliloc Arguments Length (0 for attributes)
	uint8_t[*]    Primary Cliloc Arguments
	word    Secondary Cliloc Arguments Length (0 for attributes)
	uint8_t[*]    Secondary Cliloc Arguments
	word    Third Cliloc Arguments Length (0 for attributes)
	uint8_t[*]    Third Cliloc Arguments
	endloop    Buffs <<<<
	*/

    const int tableCount = 126;

    static const uint16_t table[tableCount] = {
        0x754C, 0x754A, 0x0000, 0x0000, 0x755E, 0x7549, 0x7551, 0x7556, 0x753A, 0x754D, 0x754E,
        0x7565, 0x753B, 0x7543, 0x7544, 0x7546, 0x755C, 0x755F, 0x7566, 0x7554, 0x7540, 0x7568,
        0x754F, 0x7550, 0x7553, 0x753E, 0x755D, 0x7563, 0x7562, 0x753F, 0x7559, 0x7557, 0x754B,
        0x753D, 0x7561, 0x7558, 0x755B, 0x7560, 0x7541, 0x7545, 0x7552, 0x7569, 0x7548, 0x755A,
        0x753C, 0x7547, 0x7567, 0x7542, 0x758A, 0x758B, 0x758C, 0x758D, 0x0000, 0x758E, 0x094B,
        0x094C, 0x094D, 0x094E, 0x094F, 0x0950, 0x753E, 0x5011, 0x7590, 0x7591, 0x7592, 0x7593,
        0x7594, 0x7595, 0x7596, 0x7598, 0x7599, 0x759B, 0x759C, 0x759E, 0x759F, 0x75A0, 0x75A1,
        0x75A3, 0x75A4, 0x75A5, 0x75A6, 0x75A7, 0x75C0, 0x75C1, 0x75C2, 0x75C3, 0x75C4, 0x75F2,
        0x75F3, 0x75F4, 0x75F5, 0x75F6, 0x75F7, 0x75F8, 0x75F9, 0x75FA, 0x75FB, 0x75FC, 0x75FD,
        0x75FE, 0x75FF, 0x7600, 0x7601, 0x7602, 0x7603, 0x7604, 0x7605, 0x7606, 0x7607, 0x7608,
        0x7609, 0x760A, 0x760B, 0x760C, 0x760D, 0x760E, 0x760F, 0x7610, 0x7611, 0x7612, 0x7613,
        0x7614, 0x7615, 0x75C5, 0x75F6, 0x761B
    };

    const uint16_t buffIconStart = 0x03E9; //0x03EA ???

    uint32_t serial = ReadUInt32BE();
    uint16_t iconID = ReadUInt16BE() - buffIconStart;
    if (iconID < tableCount) //buff
    {
        CGumpBuff *gump = (CGumpBuff *)g_GumpManager.UpdateGump(0, 0, GT_BUFF);

        if (gump != nullptr)
        {
            uint16_t mode = ReadUInt16BE();
            if (mode != 0u)
            {
                Move(12);

                uint16_t timer = ReadUInt16BE();
                Move(3);

                uint32_t titleCliloc = ReadUInt32BE();
                uint32_t descriptionCliloc = ReadUInt32BE();
                uint32_t wtfCliloc = ReadUInt32BE();
                Move(4);

                wstring title = g_ClilocManager.Cliloc(g_Language)->GetW(titleCliloc, true);
                wstring description = {};
                wstring wtf = {};

                if (descriptionCliloc != 0u)
                {
                    wstring arguments = ReadWStringLE();
                    description = L'\n' + g_ClilocManager.ParseArgumentsToClilocString(
                                              descriptionCliloc, true, arguments);

                    if (description.length() < 2)
                    {
                        description = {};
                    }
                }

                if (wtfCliloc != 0u)
                {
                    wtf = L'\n' + g_ClilocManager.Cliloc(g_Language)->GetW(wtfCliloc, true);
                }

                wstring text = L"<left>" + title + description + wtf + L"</left>";
                gump->AddBuff(table[iconID], timer, text);
            }
            else
            {
                gump->DeleteBuff(table[iconID]);
            }
        }
    }
}

PACKET_HANDLER(SecureTrading)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint8_t type = ReadUInt8();
    uint32_t serial = ReadUInt32BE();

    if (type == 0) //Новое трэйд окно
    {
        uint32_t id1 = ReadUInt32BE();
        uint32_t id2 = ReadUInt32BE();
        uint8_t hasName = ReadUInt8();

        CGumpSecureTrading *gump = new CGumpSecureTrading(id1, 0, 0, id1, id2);

        CGameObject *obj = g_World->FindWorldObject(id1);

        if (obj != nullptr)
        {
            obj = obj->GetTopObject()->FindSecureTradeBox();

            if (obj != nullptr)
            {
                obj->Clear();
            }
        }

        obj = g_World->FindWorldObject(id2);

        if (obj != nullptr)
        {
            obj = obj->GetTopObject()->FindSecureTradeBox();

            if (obj != nullptr)
            {
                obj->Clear();
            }
        }

        if ((hasName != 0u) && (*Ptr != 0u))
        {
            gump->Text = ReadString();
        }

        g_GumpManager.AddGump(gump);
    }
    else if (type == 1)
    { //Отмена
        g_GumpManager.CloseGump(serial, 0, GT_TRADE);
    }
    else if (type == 2) //Обновление
    {
        CGumpSecureTrading *gump =
            (CGumpSecureTrading *)g_GumpManager.UpdateGump(serial, 0, GT_TRADE);

        if (gump != nullptr)
        {
            uint32_t id1 = ReadUInt32BE();
            uint32_t id2 = ReadUInt32BE();

            gump->StateMy = (id1 != 0);
            gump->StateOpponent = (id2 != 0);
        }
    }
}

PACKET_HANDLER(TextEntryDialog)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    uint8_t parentID = ReadUInt8();
    uint8_t buttonID = ReadUInt8();

    short textLen = ReadInt16BE();
    string text = ReadString(textLen);

    bool haveCancel = ReadUInt8() == 0u;
    uint8_t variant = ReadUInt8();
    int maxLength = ReadUInt32BE();

    short descLen = ReadInt16BE();
    string desc = ReadString(descLen);

    CGumpTextEntryDialog *gump =
        new CGumpTextEntryDialog(serial, 143, 172, variant, maxLength, text, desc);
    gump->NoClose = haveCancel;
    gump->ParentID = parentID;
    gump->ButtonID = buttonID;

    g_GumpManager.AddGump(gump);
}

PACKET_HANDLER(OpenMenu)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    uint32_t id = ReadUInt16BE();

    uint8_t nameLen = ReadUInt8();
    string name = ReadString(nameLen);

    uint8_t count = ReadUInt8();

    if (unpack16(Ptr) != 0u) //menu
    {
        CGumpMenu *gump = new CGumpMenu(serial, id, 0, 0);

        gump->Add(new CGUIGumppic(0x0910, 0, 0));

        gump->Add(new CGUIColoredPolygone(0, 0, 40, 42, 217, 49, 0xFF000001));

        CGUIText *text = (CGUIText *)gump->Add(new CGUIText(0x0386, 39, 18));
        text->CreateTextureA(1, name, 200, TS_LEFT, UOFONT_FIXED);

        CGUIHTMLGump *htmlGump = (CGUIHTMLGump *)gump->Add(
            new CGUIHTMLGump(CGumpMenu::ID_GM_HTMLGUMP, 0, 40, 42, 217, 49, false, true));
        htmlGump->Initalize(true);

        htmlGump->Add(new CGUIShader(&g_ColorizerShader, true));

        int posX = 0;

        for (int i = 0; i < count; i++)
        {
            uint16_t graphic = ReadUInt16BE();
            uint16_t color = ReadUInt16BE();

            nameLen = ReadUInt8();
            name = ReadString(nameLen);

            Wisp::CSize size = g_Orion.GetStaticArtDimension(graphic);

            if ((size.Width != 0) && (size.Height != 0))
            {
                int posY = size.Height;

                if (posY >= 47)
                {
                    posY = 0;
                }
                else
                {
                    posY = ((47 - posY) / 2);
                }

                CGUIMenuObject *menuObject = (CGUIMenuObject *)htmlGump->Add(
                    new CGUIMenuObject((int)i + 1, graphic, color, posX, posY, name));

                posX += size.Width;
            }
        }

        htmlGump->Add(new CGUIShader(&g_ColorizerShader, false));

        htmlGump->CalculateDataSize();

        gump->m_TextObject = (CGUIText *)gump->Add(new CGUIText(0x0386, 42, 105));
        //gump->m_TextObject->CreateTextureA(1, name, 200, TS_LEFT, UOFONT_FIXED); //На данный момнт создавать нечего

        g_GumpManager.AddGump(gump);
    }
    else //gray menu
    {
        int x = (g_OrionWindow.GetSize().Width / 2) - 200;
        int y = (g_OrionWindow.GetSize().Height / 2) - ((121 + (count * 21)) / 2);

        CGumpGrayMenu *gump = new CGumpGrayMenu(serial, id, x, y);

        CGUIResizepic *background =
            (CGUIResizepic *)gump->Add(new CGUIResizepic(0, 0x13EC, 0, 0, 400, 11111));

        CGUIText *text = (CGUIText *)gump->Add(new CGUIText(0x0386, 20, 16));
        text->CreateTextureA(1, name, 370);

        int offsetY = 35 + text->m_Texture.Height;
        int gumpHeight = 70 + offsetY;

        for (int i = 0; i < count; i++)
        {
            Move(4);

            nameLen = ReadUInt8();
            name = ReadString(nameLen);

            gump->Add(new CGUIRadio((int)i + 1, 0x138A, 0x138B, 0x138A, 20, offsetY)); //Button

            offsetY += 2;

            text = (CGUIText *)gump->Add(new CGUIText(0x0386, 50, offsetY));
            text->CreateTextureA(1, name, 340);

            int addHeight = text->m_Texture.Height;

            if (addHeight < 21)
            {
                addHeight = 21;
            }

            offsetY += addHeight - 1;
            gumpHeight += addHeight;
        }

        offsetY += 5;

        gump->Add(new CGUIButton(
            CGumpGrayMenu::ID_GGM_CANCEL, 0x1450, 0x1450, 0x1451, 70, offsetY)); //CANCEL
        gump->Add(new CGUIButton(
            CGumpGrayMenu::ID_GGM_CONTINUE, 0x13B2, 0x13B2, 0x13B3, 200, offsetY)); //CONTINUE

        background->Height = gumpHeight;

        g_GumpManager.AddGump(gump);
    }
}

void CPacketManager::AddHTMLGumps(CGump *gump, vector<HTMLGumpDataInfo> &list)
{
    DEBUG_TRACE_FUNCTION;
    for (int i = 0; i < (int)list.size(); i++)
    {
        HTMLGumpDataInfo &data = list[i];

        CGUIHTMLGump *htmlGump = (CGUIHTMLGump *)gump->Add(new CGUIHTMLGump(
            data.TextID + 1,
            0x0BB8,
            data.sGumpCoords->X,
            data.sGumpCoords->Y,
            data.Width,
            data.Height,
            data.HaveBackground != 0,
            data.HaveScrollbar != 0));
        htmlGump->DrawOnly = (data.HaveScrollbar == 0);

        int width = htmlGump->Width;

        if (data.HaveScrollbar != 0)
        {
            width -= 16;
        }

        uint32_t htmlColor = 0xFFFFFFFF;
        uint16_t color = 0;

        if (data.Color != 0)
        {
            if (data.Color == 0x00FFFFFF)
            {
                htmlColor = 0xFFFFFFFE;
            }
            else
            {
                htmlColor = (g_ColorManager.Color16To32((uint16_t)data.Color) << 8) | 0xFF;
            }
        }
        else if (data.HaveBackground == 0)
        {
            color = 0xFFFF;

            if (data.HaveScrollbar == 0)
            {
                htmlColor = 0x010101FF;
            }
        }
        else
        {
            width -= 9;
            htmlColor = 0x010101FF;
        }

        CGUIHTMLText *htmlText = (CGUIHTMLText *)htmlGump->Add(new CGUIHTMLText(
            data.TextID,
            (uint8_t)(g_Config.ClientVersion >= CV_305D),
            color,
            0,
            0,
            width,
            TS_LEFT,
            /*UOFONT_BLACK_BORDER*/ 0,
            htmlColor));

        if (data.IsXMF)
        {
            htmlText->Text = g_ClilocManager.Cliloc(g_Language)->GetW(data.TextID);
            htmlText->CreateTexture(data.HaveBackground == 0);
            htmlGump->CalculateDataSize();
        }
    }

    list.clear();
}

PACKET_HANDLER(OpenGump)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    vector<HTMLGumpDataInfo> htmlGumlList;

    //TPRINT("Gump dump::\n");
    //TDUMP(buf, size);

    uint32_t serial = ReadUInt32BE();
    uint32_t id = ReadUInt32BE();
    int x = ReadInt32BE();
    int y = ReadInt32BE();

    std::unordered_map<uint32_t, GumpCoords>::iterator found = m_GumpsCoordsCache.find(id);

    if (found != m_GumpsCoordsCache.end())
    {
        x = found->second.X;
        y = found->second.Y;
    }
    else
    {
        SetCachedGumpCoords(id, x, y);
    }

    CGumpGeneric *gump = new CGumpGeneric(serial, x, y, id);

    uint16_t commandsLength = ReadUInt16BE();
    string commands = ReadString(commandsLength);

    Wisp::CTextFileParser parser({}, " ", "", "{}");
    Wisp::CTextFileParser cmdParser({}, " ", "", "");
    Wisp::CTextFileParser tilepicGraphicParser({}, ",", "", "");

    vector<string> commandList = parser.GetTokens(commands.c_str());
    CBaseGUI *lastGumpObject = nullptr;

    bool EntryChanged = false;
    int FirstPage = 0;
    int CurrentPage = 0;
    CEntryText *ChangeEntry;

    for (const string &str : commandList)
    {
        vector<string> list = cmdParser.GetTokens(str.c_str());

        int listSize = (int)list.size();

        if (listSize == 0)
        {
            continue;
        }

        string cmd = ToLowerA(list[0]);

        CBaseGUI *go = nullptr;

        if (cmd == "nodispose")
        {
        }
        else if (cmd == "nomove")
        {
            gump->NoMove = true;
        }
        else if (cmd == "noclose")
        {
            gump->NoClose = true;
        }
        else if (cmd == "page")
        {
            if (listSize >= 2)
            {
                AddHTMLGumps(gump, htmlGumlList);

                int page = ToInt(list[1]);
                go = new CGUIPage(page);
                if (FirstPage == 0)
                {
                    FirstPage = page;
                }
                CurrentPage = page;
            }
        }
        else if (cmd == "group")
        {
            if (listSize >= 2)
            {
                int group = ToInt(list[1]);
                go = new CGUIGroup(group);
            }
        }
        else if (cmd == "endgroup")
        {
            go = new CGUIGroup(0);
        }
        else if (cmd == "resizepic")
        {
            if (listSize >= 6)
            {
                int x = ToInt(list[1]);
                int y = ToInt(list[2]);
                int graphic = ToInt(list[3]);
                int width = ToInt(list[4]);
                int height = ToInt(list[5]);

                go = new CGUIResizepic(0, graphic, x, y, width, height);
                go->DrawOnly = true;
            }
        }
        else if (cmd == "checkertrans")
        {
            if (listSize >= 5)
            {
                int x = ToInt(list[1]);
                int y = ToInt(list[2]);
                int width = ToInt(list[3]);
                int height = ToInt(list[4]);

                go = new CGUIChecktrans(x, y, width, height);
            }
        }
        else if (cmd == "button")
        {
            if (listSize >= 5)
            {
                int x = ToInt(list[1]);
                int y = ToInt(list[2]);
                int up = ToInt(list[3]);
                int down = ToInt(list[4]);
                int action = 0;
                int toPage = 0;
                int index = 0;

                if (listSize >= 6)
                {
                    action = ToInt(list[5]);
                }
                if (listSize >= 7)
                {
                    toPage = ToInt(list[6]);
                }
                if (listSize >= 8)
                {
                    index = ToInt(list[7]);
                }

                if (action != 0)
                {
                    toPage = -1;
                }

                go = new CGUIButton(index, up, up, down, x, y);
                go->CheckPolygone = true;

                ((CGUIButton *)go)->ToPage = toPage;
            }
        }
        else if (cmd == "buttontileart")
        {
            if (listSize >= 12)
            {
                int x = ToInt(list[1]);
                int y = ToInt(list[2]);
                int up = ToInt(list[3]);
                int down = ToInt(list[4]);
                int action = ToInt(list[5]);
                int toPage = ToInt(list[6]);
                int index = ToInt(list[7]);
                int tileGraphic = ToInt(list[8]);
                int tileColor = ToInt(list[9]);
                int tileX = ToInt(list[10]);
                int tileY = ToInt(list[11]);

                if (action != 0)
                {
                    toPage = -1;
                }

                go = new CGUIButtonTileart(
                    index, up, up, down, x, y, tileGraphic, tileColor, tileX, tileY);

                ((CGUIButton *)go)->ToPage = toPage;
            }
        }
        else if (cmd == "checkbox")
        {
            if (listSize >= 5)
            {
                int x = ToInt(list[1]);
                int y = ToInt(list[2]);
                int up = ToInt(list[3]);
                int down = ToInt(list[4]);
                int state = 0;
                int index = 0;

                if (listSize >= 6)
                {
                    state = ToInt(list[5]);
                }
                if (listSize >= 7)
                {
                    index = ToInt(list[6]);
                }

                go = new CGUICheckbox(index, up, down, up, x, y);

                ((CGUICheckbox *)go)->Checked = (state != 0);
            }
        }
        else if (cmd == "radio")
        {
            if (listSize >= 5)
            {
                int x = ToInt(list[1]);
                int y = ToInt(list[2]);
                int up = ToInt(list[3]);
                int down = ToInt(list[4]);
                int state = 0;
                int index = 0;

                if (listSize >= 6)
                {
                    state = ToInt(list[5]);
                }
                if (listSize >= 7)
                {
                    index = ToInt(list[6]);
                }

                go = new CGUIRadio(index, up, down, up, x, y);

                ((CGUIRadio *)go)->Checked = (state != 0);
            }
        }
        else if (cmd == "text")
        {
            if (listSize >= 5)
            {
                int x = ToInt(list[1]);
                int y = ToInt(list[2]);
                int color = ToInt(list[3]);
                int index = ToInt(list[4]);

                if (color != 0)
                {
                    color++;
                }

                go = new CGUIGenericText(index, color, x, y);
                go->DrawOnly = true;
            }
        }
        else if (cmd == "croppedtext")
        {
            if (listSize >= 7)
            {
                int x = ToInt(list[1]);
                int y = ToInt(list[2]);
                int width = ToInt(list[3]);
                int height = ToInt(list[4]);
                int color = ToInt(list[5]);
                int index = ToInt(list[6]);

                if (color != 0)
                {
                    color++;
                }

                go = new CGUIGenericText(index, color, x, y, width);
                go->DrawOnly = true;
            }
        }
        else if (cmd == "textentry")
        {
            if (listSize >= 8)
            {
                int x = ToInt(list[1]);
                int y = ToInt(list[2]);
                int width = ToInt(list[3]);
                int height = ToInt(list[4]);
                int color = ToInt(list[5]);
                int index = ToInt(list[6]);
                int textIndex = ToInt(list[7]);

                //if (color)
                //	color++;

                gump->Add(new CGUIHitBox(index + 1, x, y, width, height));
                gump->Add(new CGUIScissor(true, x, y, 0, 0, width, height));
                go = new CGUIGenericTextEntry(index + 1, textIndex, color, x, y);
                ((CGUIGenericTextEntry *)go)->CheckOnSerial = true;
                ((CGUITextEntry *)go)->m_Entry.Width = width;
                if (!EntryChanged)
                {
                    if (CurrentPage == 0 || CurrentPage == FirstPage)
                    {
                        ChangeEntry = &((CGUITextEntry *)go)->m_Entry;
                        EntryChanged = true;
                    }
                }
            }
        }
        else if (cmd == "textentrylimited")
        {
            if (listSize >= 9)
            {
                int x = ToInt(list[1]);
                int y = ToInt(list[2]);
                int width = ToInt(list[3]);
                int height = ToInt(list[4]);
                int color = ToInt(list[5]);
                int index = ToInt(list[6]);
                int textIndex = ToInt(list[7]);
                int length = ToInt(list[8]);

                //if (color)
                //	color++;

                gump->Add(new CGUIHitBox(index + 1, x, y, width, height));
                gump->Add(new CGUIScissor(true, x, y, 0, 0, width, height));
                go = new CGUIGenericTextEntry(index + 1, textIndex, color, x, y, width, length);
                ((CGUIGenericTextEntry *)go)->CheckOnSerial = true;
                ((CGUITextEntry *)go)->m_Entry.Width = width;
                if (!EntryChanged)
                {
                    if (CurrentPage == 0 || CurrentPage == FirstPage)
                    {
                        ChangeEntry = &((CGUITextEntry *)go)->m_Entry;
                        EntryChanged = true;
                    }
                }
            }
        }
        else if (cmd == "tilepic" || cmd == "tilepichue")
        {
            if (listSize >= 4)
            {
                int x = ToInt(list[1]);
                int y = ToInt(list[2]);
                int color = 0;
                int graphic = 0;

                if (cmd == "tilepic")
                {
                    vector<string> graphicList = tilepicGraphicParser.GetTokens(list[3].c_str());

                    if (!graphicList.empty())
                    {
                        graphic = ToInt(graphicList[0]);

                        if (graphicList.size() >= 2)
                        {
                            color = ToInt(graphicList[1]);
                        }
                    }
                }
                else
                {
                    graphic = ToInt(list[3]);
                }

                if (listSize >= 5)
                {
                    color = ToInt(list[4]);
                }

                if (color != 0)
                {
                    gump->Add(new CGUIShader(&g_ColorizerShader, true));
                }

                go = new CGUITilepic(graphic, color, x, y);
                go->DrawOnly = true;
            }
        }
        else if (cmd == "gumppic")
        {
            if (listSize >= 4)
            {
                int x = ToInt(list[1]);
                int y = ToInt(list[2]);
                int graphic = ToInt(list[3]);
                int color = 0;

                if (listSize >= 5 && g_Config.ClientVersion >= CV_305D)
                {
                    Wisp::CTextFileParser gumppicParser({}, "=", "", "");
                    vector<string> hueList = gumppicParser.GetTokens(list[4].c_str());

                    if (hueList.size() > 1)
                    {
                        color = ToInt(hueList[1]);
                    }
                    else
                    {
                        color = ToInt(hueList[0]);
                    }

                    if (listSize >= 6)
                    {
                        vector<string> classList = gumppicParser.GetTokens(list[5].c_str());

                        if (hueList.size() > 1)
                        {
                            if (ToLowerA(classList[0]) == "class" &&
                                ToLowerA(Trim(classList[1])) == "virtuegumpitem")
                            {
                                go = new CGUIVirtureGump(graphic, x, y);
                            }
                        }
                    }
                }

                if (go == nullptr)
                {
                    if (color != 0)
                    {
                        gump->Add(new CGUIShader(&g_ColorizerShader, true));
                    }

                    go = new CGUIGumppic(graphic, x, y);
                    go->Color = color;
                    go->DrawOnly = true;
                }
            }
        }
        else if (cmd == "gumppictiled")
        {
            if (listSize >= 6)
            {
                int x = ToInt(list[1]);
                int y = ToInt(list[2]);
                int width = ToInt(list[3]);
                int height = ToInt(list[4]);
                int graphic = ToInt(list[5]);

                go = new CGUIGumppicTiled(graphic, x, y, width, height);
                go->DrawOnly = true;
            }
        }
        else if (cmd == "htmlgump" || cmd == "xmfhtmlgump" || cmd == "xmfhtmlgumpcolor")
        {
            if (listSize >= 8)
            {
                HTMLGumpDataInfo htmlInfo = {};
                htmlInfo.IsXMF = (cmd != "htmlgump");
                GumpCoords *gumpCoords = new GumpCoords{ ToInt(list[1]), ToInt(list[2]) };
                htmlInfo.sGumpCoords = gumpCoords;
                htmlInfo.Width = ToInt(list[3]);
                htmlInfo.Height = ToInt(list[4]);
                htmlInfo.TextID = ToInt(list[5]);
                htmlInfo.HaveBackground = ToInt(list[6]);
                htmlInfo.HaveScrollbar = ToInt(list[7]);
                htmlInfo.Color = 0;

                if (cmd == "xmfhtmlgumpcolor" && listSize >= 9)
                {
                    htmlInfo.Color = ToInt(list[8]);

                    if (htmlInfo.Color == 0x7FFF)
                    {
                        htmlInfo.Color = 0x00FFFFFF;
                    }
                }

                htmlGumlList.push_back(htmlInfo);
            }
        }
        else if (cmd == "xmfhtmltok")
        {
            if (listSize >= 9)
            {
                HTMLGumpDataInfo htmlInfo = {};
                htmlInfo.IsXMF = true;
                GumpCoords *gumpCoords = new GumpCoords{ ToInt(list[1]), ToInt(list[2]) };
                htmlInfo.sGumpCoords = gumpCoords;
                htmlInfo.Width = ToInt(list[3]);
                htmlInfo.Height = ToInt(list[4]);
                htmlInfo.HaveBackground = ToInt(list[5]);
                htmlInfo.HaveScrollbar = ToInt(list[6]);
                htmlInfo.Color = ToInt(list[7]);

                if (htmlInfo.Color == 0x7FFF)
                {
                    htmlInfo.Color = 0x00FFFFFF;
                }

                htmlInfo.TextID = ToInt(list[8]);

                if (listSize >= 10)
                {
                }

                htmlGumlList.push_back(htmlInfo);
            }
        }
        else if (cmd == "tooltip")
        {
            if (listSize >= 2 && lastGumpObject != nullptr)
            {
                lastGumpObject->ClilocID = ToInt(list[1]);
            }
        }
        else if (cmd == "mastergump")
        {
            if (listSize >= 2)
            {
                gump->MasterGump = ToInt(list[1]);
            }
        }

        if (go != nullptr)
        {
            lastGumpObject = go;
            gump->Add(go);

            if ((go->Type == GOT_TILEPIC || go->Type == GOT_GUMPPIC) && (go->Color != 0u))
            {
                gump->Add(new CGUIShader(&g_ColorizerShader, false));
            }
            else if (go->Type == GOT_TEXTENTRY)
            {
                gump->Add(new CGUIScissor(false));
            }
        }
    }

    AddHTMLGumps(gump, htmlGumlList);

    int textLinesCount = ReadInt16BE();

    for (int i = 0; i < textLinesCount; i++)
    {
        int linelen = ReadInt16BE();

        if (linelen != 0)
        {
            gump->AddText((int)i, ReadWStringBE(linelen));
        }
        else
        {
            gump->AddText((int)i, {});
        }
    }

    if (EntryChanged)
    {
        g_EntryPointer = ChangeEntry;
    }

    g_GumpManager.AddGump(gump);
}

PACKET_HANDLER(OpenCompressedGump)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t senderID = ReadUInt32BE();
    uint32_t gumpID = ReadUInt32BE();
    uint32_t x = ReadUInt32BE();
    uint32_t y = ReadUInt32BE();
    uLongf cLen = ReadUInt32BE() - 4; //Compressed Length (4 == sizeof(DecompressedLen)
    uLongf dLen = ReadUInt32BE();     //Decompressed Length

    if (cLen < 1)
    {
        LOG("CLen=%d\nServer Sends bad Compressed Gumpdata!\n", cLen);

        return;
    }
    if ((int)(28 + cLen) > Size)
    {
        LOG("Server Sends bad Compressed Gumpdata!\n");

        return;
    }

    // Layout data.....
    vector<uint8_t> decLayoutData(dLen);
    LOG("Gump layout:\n\tSenderID=0x%08X\n\tGumpID=0x%08X\n\tCLen=%d\n\tDLen=%d\nDecompressing layout gump data...\n",
        senderID,
        gumpID,
        cLen,
        dLen);

    int z_err = mz_uncompress(&decLayoutData[0], &dLen, Ptr, cLen);
    if (z_err != Z_OK)
    {
        LOG("Decompress layout gump error %d\n", z_err);
        return;
    }

    LOG("Layout gump data decompressed!\n");
    // Text data.....

    Move(cLen);

    uint32_t linesCount = ReadUInt32BE(); //Text lines count
    uint32_t cTLen = 0;
    uLongf dTLen = 0;
    vector<uint8_t> gumpDecText;

    if (linesCount > 0)
    {
        cTLen = ReadUInt32BE(); //Compressed lines length
        dTLen = ReadUInt32BE(); //Decompressed lines length

        gumpDecText.resize(dTLen);
        LOG("Decompressing text gump data...\n");

        z_err = mz_uncompress(&gumpDecText[0], &dTLen, Ptr, cTLen);
        if (z_err != Z_OK)
        {
            LOG("Decompress text gump error %d\n", z_err);
            return;
        }

        LOG("Text gump data decompressed!\nGump text lines:\n\tLinesCount=%d\n\tCTLen=%d\n\tDTLen=%d\n",
            linesCount,
            cTLen,
            dTLen);
    }

    int newsize = 21 + dLen + 2 + dTLen;

    vector<uint8_t> newbufData(newsize);
    uint8_t *newbuf = &newbufData[0];
    newbuf[0] = 0xb0;
    pack16(newbuf + 1, newsize);
    pack32(newbuf + 3, senderID);
    pack32(newbuf + 7, gumpID);
    pack32(newbuf + 11, x);
    pack32(newbuf + 15, y);
    pack16(newbuf + 19, (uint16_t)dLen);
    memcpy(newbuf + 21, &decLayoutData[0], dLen);
    pack16(newbuf + 21 + dLen, (uint16_t)linesCount);

    if (linesCount > 0)
    {
        memcpy(newbuf + 23 + dLen, &gumpDecText[0], dTLen);
    }
    else
    {
        newbuf[newsize - 1] = 0x00;
    }

    LOG("Gump decompressed! newsize=%d\n", newsize);

    Size = newsize;
    Start = newbuf;
    End = Start + Size;

    OnPacket();
}

PACKET_HANDLER(DyeData)
{
    DEBUG_TRACE_FUNCTION;
    uint32_t serial = ReadUInt32BE();
    Move(2);
    uint16_t graphic = ReadUInt16BE();

    Wisp::CSize gumpSize = g_Orion.GetGumpDimension(0x0906);

    auto x = int16_t((g_OrionWindow.GetSize().Width / 2) - (gumpSize.Width / 2));
    auto y = int16_t((g_OrionWindow.GetSize().Height / 2) - (gumpSize.Height / 2));
    CGumpDye *gump = new CGumpDye(serial, x, y, graphic);

    g_GumpManager.AddGump(gump);
}

PACKET_HANDLER(DisplayMap)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    uint16_t gumpid = ReadUInt16BE();

    //TPRINT("gumpid = 0x%04X\n", gumpid);

    uint16_t startX = ReadUInt16BE();
    uint16_t startY = ReadUInt16BE();
    uint16_t endX = ReadUInt16BE();
    uint16_t endY = ReadUInt16BE();
    uint16_t width = ReadUInt16BE();
    uint16_t height = ReadUInt16BE();

    CGumpMap *gump = new CGumpMap(serial, gumpid, startX, startY, endX, endY, width, height);

    if (*Start == 0xF5 || g_Config.ClientVersion >= CV_308Z) //308z или выше?
    {
        uint16_t facet = 0;

        if (*Start == 0xF5)
        {
            facet = ReadUInt16BE();
        }

        g_MultiMap.LoadFacet(gump, gump->m_Texture, facet);
    }
    else
    {
        g_MultiMap.LoadMap(gump, gump->m_Texture);
    }

    //TPRINT("GumpX=%d GumpY=%d\n", startX, startY);
    //TPRINT("GumpTX=%d GumpTY=%d\n", endX, endY);
    //TPRINT("GumpW=%d GumpH=%d\n", width, height);

    g_GumpManager.AddGump(gump);

    CGameItem *obj = g_World->FindWorldItem(serial);

    if (obj != nullptr)
    {
        obj->Opened = true;
    }
}

PACKET_HANDLER(MapData)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    CGumpMap *gump = (CGumpMap *)g_GumpManager.UpdateGump(serial, 0, GT_MAP);

    if (gump != nullptr && gump->m_DataBox != nullptr)
    {
        switch ((MAP_MESSAGE)ReadUInt8()) //Action
        {
            case MM_ADD: //Add Pin
            {
                Move(1);

                short x = ReadUInt16BE();
                short y = ReadUInt16BE();

                gump->m_DataBox->Add(new CGUIGumppic(0x139B, x, y));
                gump->WantRedraw = true;

                break;
            }
            case MM_INSERT: //Insert New Pin
            {
                break;
            }
            case MM_MOVE: //Change Pin
            {
                break;
            }
            case MM_REMOVE: //Remove Pin
            {
                break;
            }
            case MM_CLEAR: //Clear Pins
            {
                gump->m_DataBox->Clear();
                gump->WantRedraw = true;

                break;
            }
            case MM_EDIT_RESPONSE: //Reply From Server to Action 6 (Plotting request)
            {
                gump->SetPlotState(ReadUInt8());

                break;
            }
            default:
                break;
        }
    }
}

PACKET_HANDLER(TipWindow)
{
    DEBUG_TRACE_FUNCTION;
    uint8_t flag = ReadUInt8();

    if (flag != 1) //1 - ignore
    {
        uint32_t serial = ReadUInt32BE();
        short len = ReadInt16BE();

        string str = ReadString(len);

        int x = 20;
        int y = 20;

        if (flag == 0u)
        {
            x = 200;
            y = 100;
        }

        CGumpTip *gump = new CGumpTip(serial, x, y, str, flag != 0);

        g_GumpManager.AddGump(gump);
    }
}

PACKET_HANDLER(CharacterProfile)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    wstring topText = ToWString(ReadString());
    wstring bottomText = ReadWStringBE();
    wstring dataText = ReadWStringBE();
    CGumpProfile *gump = new CGumpProfile(serial, 170, 90, topText, bottomText, dataText);
    g_GumpManager.AddGump(gump);
}

PACKET_HANDLER(BulletinBoardData)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    switch (ReadUInt8())
    {
        case 0: //Open board
        {
            uint32_t serial = ReadUInt32BE();

            CGameItem *item = g_World->FindWorldItem(serial);

            if (item != nullptr)
            {
                CGumpBulletinBoard *bbGump =
                    (CGumpBulletinBoard *)g_GumpManager.UpdateGump(serial, 0, GT_BULLETIN_BOARD);

                if (bbGump != nullptr)
                {
                    CBaseGUI *bbItem = (CBaseGUI *)bbGump->m_HTMLGump->m_Items;

                    while (bbItem != nullptr)
                    {
                        CBaseGUI *bbNext = (CBaseGUI *)bbItem->m_Next;

                        if (bbItem->Type == GOT_BB_OBJECT)
                        {
                            bbGump->m_HTMLGump->Delete(bbItem);
                        }

                        bbItem = bbNext;
                    }

                    bbGump->m_HTMLGump->CalculateDataSize();
                }

                item->Opened = true;
            }

            string str((char *)Ptr);

            int x = (g_OrionWindow.GetSize().Width / 2) - 245;
            int y = (g_OrionWindow.GetSize().Height / 2) - 205;

            CGumpBulletinBoard *gump = new CGumpBulletinBoard(serial, x, y, str);

            g_GumpManager.AddGump(gump);

            break;
        }
        case 1: //Summary message
        {
            uint32_t boardSerial = ReadUInt32BE();

            CGumpBulletinBoard *gump =
                (CGumpBulletinBoard *)g_GumpManager.GetGump(boardSerial, 0, GT_BULLETIN_BOARD);

            if (gump != nullptr)
            {
                uint32_t serial = ReadUInt32BE();
                uint32_t parentID = ReadUInt32BE();

                //poster
                int len = ReadUInt8();
                wstring text = (len > 0 ? DecodeUTF8(ReadString(len)) : L"") + L" - ";

                //subject
                len = ReadUInt8();
                text += (len > 0 ? DecodeUTF8(ReadString(len)) : L"") + L" - ";

                //data time
                len = ReadUInt8();
                text += (len > 0 ? DecodeUTF8(ReadString(len)) : L"");

                int posY = (gump->m_HTMLGump->GetItemsCount() - 5) * 18;

                if (posY < 0)
                {
                    posY = 0;
                }

                gump->m_HTMLGump->Add(new CGUIBulletinBoardObject(serial, 0, posY, text));
                gump->m_HTMLGump->CalculateDataSize();
            }

            break;
        }
        case 2: //Message
        {
            uint32_t boardSerial = ReadUInt32BE();

            CGumpBulletinBoard *gump =
                (CGumpBulletinBoard *)g_GumpManager.GetGump(boardSerial, 0, GT_BULLETIN_BOARD);

            if (gump != nullptr)
            {
                uint32_t serial = ReadUInt32BE();

                //poster
                int len = ReadUInt8();
                wstring poster = (len > 0 ? DecodeUTF8(ReadString(len)) : L"");

                //subject
                len = ReadUInt8();
                wstring subject = (len > 0 ? DecodeUTF8(ReadString(len)) : L"");

                //data time
                len = ReadUInt8();
                wstring dataTime = (len > 0 ? DecodeUTF8(ReadString(len)) : L"");

                //unused, in old clients: user's graphic, color
                Move(4);

                uint8_t unknown = ReadUInt8();

                if (unknown > 0)
                {
                    //unused data
                    Move(unknown * 4);
                }

                uint8_t lines = ReadUInt8();
                wstring data = {};

                for (int i = 0; i < lines; i++)
                {
                    uint8_t linelen = ReadUInt8();

                    if (data.length() != 0u)
                    {
                        data += L"\n";
                    }

                    if (linelen > 0)
                    {
                        data += DecodeUTF8(ReadString(linelen));
                    }
                }

                uint8_t variant = 1 + (int)(poster == ToWString(g_Player->GetName()));
                g_GumpManager.AddGump(new CGumpBulletinBoardItem(
                    serial, 0, 0, variant, boardSerial, poster, subject, dataTime, data));
            }

            break;
        }
        default:
            break;
    }
}

PACKET_HANDLER(OpenBook) // 0x93
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    uint8_t flags = ReadUInt8();
    Move(1);
    auto pageCount = ReadUInt16BE();
    CGumpBook *gump =
        new CGumpBook(serial, 0, 0, pageCount, flags != 0, (g_Config.ClientVersion >= CV_308Z));

    gump->m_EntryTitle->m_Entry.SetTextA(ReadString(60));
    gump->m_EntryAuthor->m_Entry.SetTextA(ReadString(30));

    g_GumpManager.AddGump(gump);
}

PACKET_HANDLER(OpenBookNew) // 0xD4
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();
    uint8_t flag1 = ReadUInt8();
    uint8_t flag2 = ReadUInt8();
    uint16_t pageCount = ReadUInt16BE();

    CGumpBook *gump = new CGumpBook(serial, 0, 0, pageCount, (flag1 + flag2) != 0, true);

    int titleLen = ReadUInt16BE();

    if (titleLen > 0)
    {
        gump->m_EntryTitle->m_Entry.SetTextA(ReadString(titleLen));
    }

    int authorLen = ReadUInt16BE();

    if (authorLen > 0)
    {
        gump->m_EntryAuthor->m_Entry.SetTextA(ReadString(authorLen));
    }

    g_GumpManager.AddGump(gump);
}

PACKET_HANDLER(BookData)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();

    CGumpBook *gump = (CGumpBook *)g_GumpManager.GetGump(serial, 0, GT_BOOK);

    if (gump != nullptr)
    {
        uint16_t pageCount = ReadUInt16BE();

        for (int i = 0; i < pageCount; i++)
        {
            uint16_t page = ReadUInt16BE();

            if (page > gump->PageCount)
            {
                continue;
            }

            uint16_t lineCount = ReadUInt16BE();

            wstring str = {};

            for (int j = 0; j < lineCount; j++)
            {
                if (j != 0)
                {
                    str += L'\n';
                }

                wstring temp = DecodeUTF8(ReadString());

                while ((temp.length() != 0u) && (temp.back() == L'\n' || temp.back() == L'\r'))
                {
                    temp.resize(temp.length() - 1);
                }

                str += temp;
            }

            gump->SetPageData(page, str);
        }
    }
}

PACKET_HANDLER(BuyList)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();

    CGameItem *container = g_World->FindWorldItem(serial);

    if (container == nullptr)
    {
        LOG("Error!!! Buy container is not found!!!\n");
        return;
    }

    uint32_t vendorSerial = container->Container;

    CGameCharacter *vendor = g_World->FindWorldCharacter(vendorSerial);

    if (vendor == nullptr)
    {
        LOG("Error!!! Buy vendor is not found!!!\n");
        return;
    }

    CGumpShop *gump = (CGumpShop *)g_GumpManager.GetGump(vendorSerial, 0, GT_SHOP);

    if (gump != nullptr && (gump->Serial != vendorSerial || !gump->IsBuyGump))
    {
        g_GumpManager.RemoveGump(gump);
        gump = nullptr;
    }

    if (gump == nullptr)
    {
        gump = new CGumpShop(vendorSerial, true, 150, 5);
        g_GumpManager.AddGump(gump);
    }

    gump->WantRedraw = true;

    if (container->Layer == OL_BUY_RESTOCK || container->Layer == OL_BUY)
    {
        uint8_t count = ReadUInt8();

        CGameItem *item = (CGameItem *)container->m_Items;
        //oldp spams this packet twice in a row on NPC verndors. nullptr check is needed t
        if (item == nullptr)
        {
            return;
        }

        bool reverse = (item->GetX() > 1);

        if (reverse)
        {
            while (item != nullptr && item->m_Next != nullptr)
            {
                item = (CGameItem *)item->m_Next;
            }
        }

        CGUIHTMLGump *htmlGump = gump->m_ItemList[0];

        int currentY = 0;

        QFOR(shopItem, htmlGump->m_Items, CBaseGUI *)
        {
            if (shopItem->Type == GOT_SHOPITEM)
            {
                currentY += shopItem->GetSize().Height;
            }
        }

        for (int i = 0; i < count; i++)
        {
            if (item == nullptr)
            {
                LOG("Error!!! Buy item is not found!!!\n");
                break;
            }

            item->Price = ReadUInt32BE();

            uint8_t nameLen = ReadUInt8();
            string name = ReadString(nameLen);

            //try int.parse and read cliloc.
            int clilocNum = 0;

            if (Int32TryParse(name, clilocNum))
            {
                item->SetName(g_ClilocManager.Cliloc(g_Language)->GetA(clilocNum, true));
                item->NameFromCliloc = true;
            }
            else
            {
                item->SetName(name);
                item->NameFromCliloc = false;
            }

            if (reverse)
            {
                item = (CGameItem *)item->m_Prev;
            }
            else
            {
                item = (CGameItem *)item->m_Next;
            }
        }

        htmlGump->CalculateDataSize();
    }
    else
    {
        LOG("Unknown layer for buy container!!!\n");
    }
}

PACKET_HANDLER(SellList)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint32_t serial = ReadUInt32BE();

    CGameCharacter *vendor = g_World->FindWorldCharacter(serial);

    if (vendor == nullptr)
    {
        LOG("Error!!! Sell vendor is not found!!!\n");
        return;
    }

    uint16_t itemsCount = ReadUInt16BE();

    if (itemsCount == 0u)
    {
        LOG("Error!!! Sell list is empty.\n");
        return;
    }

    g_GumpManager.CloseGump(0, 0, GT_SHOP);

    CGumpShop *gump = new CGumpShop(serial, false, 100, 0);
    CGUIHTMLGump *htmlGump = gump->m_ItemList[0];

    int currentY = 0;

    for (int i = 0; i < itemsCount; i++)
    {
        uint32_t itemSerial = ReadUInt32BE();
        uint16_t graphic = ReadUInt16BE();
        uint16_t color = ReadUInt16BE();
        uint16_t count = ReadUInt16BE();
        uint16_t price = ReadUInt16BE();
        int nameLen = ReadInt16BE();
        string name = ReadString(nameLen);

        int clilocNum = 0;
        bool nameFromCliloc = false;

        if (Int32TryParse(name, clilocNum))
        {
            name = g_ClilocManager.Cliloc(g_Language)->GetA(clilocNum, true);
            nameFromCliloc = true;
        }

        CGUIShopItem *shopItem = (CGUIShopItem *)htmlGump->Add(
            new CGUIShopItem(itemSerial, graphic, color, count, price, name, 0, currentY));
        shopItem->NameFromCliloc = nameFromCliloc;

        if (i == 0)
        {
            shopItem->Selected = true;
            shopItem->CreateNameText();
            shopItem->UpdateOffsets();
        }

        currentY += shopItem->GetSize().Height;
    }

    htmlGump->CalculateDataSize();

    g_GumpManager.AddGump(gump);
}

PACKET_HANDLER(BuyReply)
{
    DEBUG_TRACE_FUNCTION;
    uint32_t serial = ReadUInt32BE();
    uint8_t flag = ReadUInt8();

    if (flag == 0u)
    { //Close shop gump
        g_GumpManager.CloseGump(serial, 0, GT_SHOP);
    }
}

PACKET_HANDLER(Logout)
{
    DEBUG_TRACE_FUNCTION;
    g_Orion.LogOut();
}

PACKET_HANDLER(OPLInfo)
{
    DEBUG_TRACE_FUNCTION;
    if (g_TooltipsEnabled)
    {
        uint32_t serial = ReadUInt32BE();
        uint32_t revision = ReadUInt32BE();

        if (!g_ObjectPropertiesManager.RevisionCheck(serial, revision))
        {
            AddMegaClilocRequest(serial);
        }
    }
}

PACKET_HANDLER(CustomHouse)
{
    DEBUG_TRACE_FUNCTION;
    bool compressed = ReadUInt8() == 0x03;
    bool enableResponse = ReadUInt8() == 0x01;
    uint32_t serial = ReadUInt32BE();
    uint32_t revision = ReadUInt32BE();
    CGameItem *foundationItem = g_World->FindWorldItem(serial);

    if (foundationItem == nullptr)
    {
        return;
    }

    CMulti *multi = foundationItem->GetMulti();

    if (multi == nullptr)
    {
        return;
    }

    ReadUInt16BE();
    ReadUInt16BE();

    CCustomHouse *house = g_CustomHousesManager.Get(serial);

    if (house == nullptr)
    {
        house = new CCustomHouse(serial, revision);
        g_CustomHousesManager.Add(house);
    }
    else
    {
        house->Revision = revision;
    }

    LOG("House update in cache: 0x%08X 0x%08X\n", serial, revision);

    house->m_Items.clear();

    short minX = multi->MinX;
    short minY = multi->MinY;
    short maxY = multi->MaxY;

    uint8_t planes = ReadUInt8();

    for (int plane = 0; plane < planes; plane++)
    {
        uint32_t header = ReadUInt32BE();
        uLongf dLen = ((header & 0xFF0000) >> 16) | ((header & 0xF0) << 4);
        int cLen = ((header & 0xFF00) >> 8) | ((header & 0x0F) << 8);
        int planeZ = (header & 0x0F000000) >> 24;
        int planeMode = (header & 0xF0000000) >> 28;

        if (cLen <= 0)
        {
            continue;
        }

        vector<uint8_t> decompressedBytes(dLen);
        int z_err = mz_uncompress(&decompressedBytes[0], &dLen, Ptr, cLen);
        if (z_err != Z_OK)
        {
            LOG("Bad CustomHouseStruct compressed data received from server, house serial:%i\n",
                serial);
            //LOG("House plane idx:%i\n", idx);
            continue;
        }

        Wisp::CDataReader tempReader(static_cast<uint8_t *>(&decompressedBytes[0]), dLen);
        Move(cLen);

        uint16_t id = 0;
        char x = 0;
        char y = 0;
        char z = 0;

        switch (planeMode)
        {
            case 0:
            {
                for (uint32_t i = 0; i < decompressedBytes.size() / 5; i++)
                {
                    id = tempReader.ReadUInt16BE();
                    x = tempReader.ReadUInt8();
                    y = tempReader.ReadUInt8();
                    z = tempReader.ReadUInt8();

                    if (id != 0)
                    {
                        house->m_Items.push_back(CBuildObject(id, x, y, z));
                    }
                }

                break;
            }
            case 1:
            {
                if (planeZ > 0)
                {
                    z = ((planeZ - 1) % 4) * 20 + 7; // Z=7,27,47,67
                }
                else
                {
                    z = 0;
                }

                for (uint32_t i = 0; i < decompressedBytes.size() / 4; i++)
                {
                    id = tempReader.ReadUInt16BE();
                    x = tempReader.ReadUInt8();
                    y = tempReader.ReadUInt8();

                    if (id != 0)
                    {
                        house->m_Items.push_back(CBuildObject(id, x, y, z));
                    }
                }

                break;
            }
            case 2:
            {
                short xOffs = 0;
                short yOffs = 0;
                short multiHeight = 0;

                if (planeZ > 0)
                {
                    z = ((planeZ - 1) % 4) * 20 + 7; // Z=7,27,47,67
                }
                else
                {
                    z = 0;
                }

                if (planeZ <= 0)
                {
                    xOffs = minX;
                    yOffs = minY;
                    multiHeight = (maxY - minY) + 2;
                }
                else if (planeZ <= 4)
                {
                    xOffs = minX + 1;
                    yOffs = minY + 1;
                    multiHeight = (maxY - minY);
                }
                else
                {
                    xOffs = minX;
                    yOffs = minY;
                    multiHeight = (maxY - minY) + 1;
                }

                for (uint32_t i = 0; i < decompressedBytes.size() / 2; i++)
                {
                    id = tempReader.ReadUInt16BE();
                    x = i / multiHeight + xOffs;
                    y = i % multiHeight + yOffs;

                    if (id != 0)
                    {
                        house->m_Items.push_back(CBuildObject(id, x, y, z));
                    }
                }

                break;
            }
            default:
                break;
        }
    }

    house->Paste(foundationItem);

    if (enableResponse)
    {
        CPacketCustomHouseResponse().Send();
    }
}

PACKET_HANDLER(OrionMessages)
{
    DEBUG_TRACE_FUNCTION;
    uint16_t command = ReadUInt16BE();
    uint8_t type = command >> 12;
    command &= 0x0FFF;

    if (type != 0u)
    {
        return;
    }

    switch (command)
    {
        case OCT_ORION_FEATURES:
        {
            g_OrionFeaturesFlags = ReadUInt32BE();
            g_ConfigManager.UpdateFeatures();
            break;
        }
        case OCT_ORION_IGNORE_TILES_IN_FILTER:
        {
            g_Orion.m_IgnoreInFilterTiles.clear();
            uint16_t count = ReadUInt16BE();
            for (int i = 0; i < count; i++)
            {
                g_Orion.m_IgnoreInFilterTiles.push_back(
                    std::pair<uint16_t, uint16_t>(ReadUInt16BE(), 0));
            }

            uint16_t countRange = ReadUInt16BE();
            for (int i = 0; i < countRange; i++)
            {
                uint16_t rangeStart = ReadUInt16BE();
                uint16_t rangeEnd = ReadUInt16BE();
                g_Orion.m_IgnoreInFilterTiles.push_back(
                    std::pair<uint16_t, uint16_t>(rangeStart, rangeEnd));
            }
            break;
        }
        case OCT_ORION_VERSION:
        {
            // FIXME: get numeric version from GitRevision.h
            CPacketOrionVersion(0).Send();
            break;
        }
        case OCT_CLOSE_GENERIC_GUMP_WITHOUT_RESPONSE:
        {
            uint32_t serial = ReadUInt32BE();
            uint32_t id = ReadUInt32BE();
            uint8_t all = ReadUInt8();
            QFOR(gump, g_GumpManager.m_Items, CGump *)
            {
                if (gump->GumpType == GT_GENERIC && gump->Serial == serial && gump->ID == id)
                {
                    gump->RemoveMark = true;
                    if (all == 0u)
                    {
                        break;
                    }
                }
            }

            break;
        }
        case OCT_SELECT_MENU:
        {
            uint32_t serial = ReadUInt32BE();
            uint32_t id = ReadUInt32BE();
            uint32_t code = ReadUInt32BE();
            if ((serial == 0u) && (id == 0u))
            {
                for (CGump *gump = (CGump *)g_GumpManager.m_Items; gump != nullptr;)
                {
                    CGump *next = (CGump *)gump->m_Next;
                    if (gump->GumpType == GT_MENU || gump->GumpType == GT_GRAY_MENU)
                    {
                        CPacketMenuResponse(gump, code).Send();
                        g_GumpManager.RemoveGump(gump);
                    }
                    gump = next;
                }

                break;
            }

            CGump *gump = g_GumpManager.GetGump(serial, id, GT_MENU);
            if (gump == nullptr)
            {
                gump = g_GumpManager.GetGump(serial, id, GT_GRAY_MENU);
                if (gump != nullptr)
                {
                    CPacketGrayMenuResponse(gump, code).Send();
                    g_GumpManager.RemoveGump(gump);
                }
            }
            else
            {
                CPacketMenuResponse(gump, code).Send();
                g_GumpManager.RemoveGump(gump);
            }
            break;
        }
        case OCT_CAST_SPELL_REQUEST:
        {
            int id = ReadUInt32BE();
            if (id >= 0)
            {
                g_LastSpellIndex = id;
                CPacketCastSpell(id).Send();
            }
            break;
        }
        case OCT_USE_SKILL_REQUEST:
        {
            int id = ReadUInt32BE();
            if (id >= 0)
            {
                g_LastSkillIndex = id;
                CPacketUseSkill(id).Send();
            }
            break;
        }
        case OCT_DRAW_STATUSBAR:
        {
            uint32_t serial = ReadUInt32BE();
            int x = ReadInt32BE();
            int y = ReadInt32BE();
            bool minimized = (ReadUInt8() != 0);
            if (serial != g_PlayerSerial)
            {
                minimized = true;
            }

            CGump *gump = g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);
            if (gump != nullptr)
            {
                gump->Minimized = minimized;

                if (gump->Minimized)
                {
                    gump->MinimizedX = x;
                    gump->MinimizedY = y;
                }
                else
                {
                    gump->SetX(x);
                    gump->SetY(y);
                }
            }
            else
            {
                CPacketStatusRequest(serial).Send();
                g_GumpManager.AddGump(new CGumpStatusbar(serial, x, y, minimized));
            }
            break;
        }
        case OCT_CLOSE_STATUSBAR:
        {
            uint32_t serial = ReadUInt32BE();
            g_GumpManager.CloseGump(serial, 0, GT_STATUSBAR);
            break;
        }
        case OCT_SECURE_TRADE_CHECK:
        {
            uint32_t id1 = ReadUInt32BE();
            CGumpSecureTrading *gump =
                (CGumpSecureTrading *)g_GumpManager.UpdateGump(id1, 0, GT_TRADE);
            if (gump != nullptr)
            {
                gump->StateMy = (ReadUInt8() != 0);
                CPacketTradeResponse(gump, 2).Send();
            }
            break;
        }
        case OCT_SECURE_TRADE_CLOSE:
        {
            uint32_t id1 = ReadUInt32BE();
            auto gump = (CGumpSecureTrading *)g_GumpManager.GetGump(id1, 0, GT_TRADE);
            if (gump != nullptr)
            {
                gump->RemoveMark = true;
                CPacketTradeResponse(gump, 1).Send();
            }
            break;
        }
        case OCT_UNICODE_SPEECH_REQUEST:
        {
            uint16_t color = ReadUInt16BE();
            wstring text = ReadWStringBE();
            CGameConsole::Send(text, color);
            break;
        }
        case OCT_RENAME_MOUNT_REQUEST:
        {
            uint32_t serial = ReadUInt32BE();
            string text = ReadString();
            CPacketRenameRequest(serial, text).Send();
            break;
        }
        case OCT_RECONNECT:
        {
            g_Orion.StartReconnect();
            break;
        }
        case OCT_PLAY_MACRO:
        {
            int count = ReadUInt16BE();
            static CMacro existsMacros(0, false, false, false);
            existsMacros.Clear();

            g_MacroPointer = nullptr;
            g_MacroManager.SendNotificationToPlugin = true;
            for (int m = 0; m < count; m++)
            {
                string name = ReadString();
                string param = ReadString();
                MACRO_CODE macroCode = MC_NONE;
                for (int i = 0; i < CMacro::MACRO_ACTION_NAME_COUNT; i++)
                {
                    std::string macroName = CMacro::m_MacroActionName[i];
                    if (strcmp(name.c_str(), macroName.c_str()) == 0)
                    {
                        macroCode = (MACRO_CODE)i;
                        break;
                    }
                }

                if (macroCode == MC_NONE)
                {
                    LOG("Invalid macro name: %s\n", name.c_str());
                    continue;
                }

                CMacroObject *macro = CMacro::CreateMacro(macroCode);
                if (param.length() != 0u)
                {
                    if (macro->HaveString())
                    {
                        ((CMacroObjectString *)macro)->m_String = param;
                    }
                    else
                    {
                        for (int i = 0; i < CMacro::MACRO_ACTION_COUNT; i++)
                        {
                            if (param == CMacro::m_MacroAction[i])
                            {
                                macro->SubCode = (MACRO_SUB_CODE)i;
                                break;
                            }
                        }
                    }
                }
                existsMacros.Add(macro);
            }

            g_MacroPointer = (CMacroObject *)existsMacros.m_Items;
            g_MacroManager.WaitingBandageTarget = false;
            g_MacroManager.WaitForTargetTimer = 0;
            g_MacroManager.Execute();
            break;
        }
        case OCT_MOVE_PAPERDOLL:
        {
            uint32_t serial = ReadUInt32BE();
            int x = ReadInt32BE();
            int y = ReadInt32BE();
            CGump *gump = g_GumpManager.UpdateContent(serial, 0, GT_PAPERDOLL);
            if (gump != nullptr)
            {
                if (gump->Minimized)
                {
                    gump->MinimizedX = x;
                    gump->MinimizedY = y;
                }
                else
                {
                    gump->SetX(x);
                    gump->SetY(y);
                }
            }
            break;
        }
        case OCT_USE_ABILITY:
        {
            CGumpAbility::OnAbilityUse(ReadUInt8() % 2);
            break;
        }
        case OCT_OPEN_DOOR:
        {
            g_Orion.OpenDoor();
            break;
        }
        default:
            break;
    }
}

PACKET_HANDLER(PacketsList)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    int count = ReadUInt16BE();
    for (int i = 0; i < count; i++)
    {
        uint8_t id = ReadUInt8();
        if (id == 0xF3)
        {
            HandleUpdateItemSA();
        }
        else
        {
            LOG("Unknown packet ID=0x%02X in packet 0xF7!!!\n", id);
            break;
        }
    }
}

PACKET_HANDLER(MovePlayer)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint8_t direction = ReadUInt8();
    g_PathFinder.Walk((direction & 0x80) != 0, direction & 7);
}

PACKET_HANDLER(Pathfinding)
{
    DEBUG_TRACE_FUNCTION;
    if (g_World == nullptr)
    {
        return;
    }

    uint16_t x = ReadInt16BE();
    uint16_t y = ReadInt16BE();
    uint16_t z = ReadInt16BE();
    g_PathFinder.WalkTo(x, y, z, 0);
}

void CPacketManager::SetCachedGumpCoords(uint32_t id, int x, int y)
{
    std::unordered_map<uint32_t, GumpCoords>::iterator found = m_GumpsCoordsCache.find(id);

    if (found != m_GumpsCoordsCache.end())
    {
        found->second.X = x;
        found->second.Y = y;
    }
    else
    {
        m_GumpsCoordsCache[id] = GumpCoords{ x, y };
    }
}

PACKET_HANDLER(BoatMoving)
{
    DEBUG_TRACE_FUNCTION;

    // FIXME: disable BoatMoving for the 0.1.9.6 patch
    return;
    uint32_t boatSerial = ReadUInt32BE();

    CGameObject *boat = g_World->FindWorldObject(boatSerial);
    if (boat == nullptr)
    {
        return;
    }

    uint8_t boatSpeed = ReadUInt8();
    uint8_t movingDirection = ReadUInt8();
    uint8_t facingDirection = ReadUInt8();
    uint16_t boatX = ReadUInt16BE();
    uint16_t boatY = ReadUInt16BE();
    uint16_t boatZ = ReadUInt16BE();
    uint16_t boatObjectsCount = ReadUInt16BE();

    g_World->UpdateGameObject(
        boatSerial,
        boat->Graphic,
        0,
        boat->Count,
        boatX,
        boatY,
        (char)boatZ,
        facingDirection,
        boat->Color,
        boat->GetFlags(),
        0,
        UGOT_MULTI,
        (uint16_t)1);

    for (uint16_t i = 0; i < boatObjectsCount; i++)
    {
        uint32_t boatObjectSerial = ReadUInt32BE();
        uint16_t boatObjectX = ReadUInt16BE();
        uint16_t boatObjectY = ReadUInt16BE();
        uint16_t boatObjectZ = ReadUInt16BE();

        CGameObject *boatObject = g_World->FindWorldObject(boatObjectSerial);
        if (boatObject == nullptr)
        {
            continue;
        }

        uint8_t direction = boatObject->NPC ? ((CGameCharacter *)boatObject)->Direction : 0;
        g_World->UpdateGameObject(
            boatObjectSerial,
            boatObject->Graphic,
            0,
            0,
            boatObjectX,
            boatObjectY,
            (char)boatObjectZ,
            direction,
            boatObject->Color,
            boatObject->GetFlags(),
            0,
            UGOT_ITEM,
            (uint16_t)1);
    }
}
