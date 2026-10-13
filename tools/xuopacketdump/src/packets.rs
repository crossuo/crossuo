// Packet table mirrored from crossuo src/Managers/PacketManager.cpp
// (CPacketManager::m_Packets) including the client-version dependent size
// overrides applied by CPacketManager::ConfigureClientVersion.
//
// Size conventions (see src/Wisp/WispPacketMessage.cpp):
//   size > 0  -> fixed size packet
//   size == 0 -> variable size, big-endian u16 length at offset 1
//                (PACKET_VARIABLE_SIZE == 0, src/Misc.h)

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Direction {
    // DIR_SEND: client -> server
    ClientToServer,
    // DIR_RECV: server -> client
    ServerToClient,
    // DIR_BOTH
    Both,
}

#[derive(Clone, Copy)]
pub struct PacketInfo {
    pub id: u8,
    pub name: &'static str,
    pub size: u16, // 0 == variable
    pub dir: Direction,
}

const C: Direction = Direction::ClientToServer;
const S: Direction = Direction::ServerToClient;
const B: Direction = Direction::Both;

macro_rules! p {
    ($id:expr, $name:expr, $size:expr, $dir:expr) => {
        PacketInfo {
            id: $id,
            name: $name,
            size: $size,
            dir: $dir,
        }
    };
    // UMSG: unnamed
    ($id:expr, _, $size:expr) => {
        PacketInfo {
            id: $id,
            name: "Unknown",
            size: $size,
            dir: B,
        }
    };
}

pub static PACKETS: [PacketInfo; 0x100] = [
    p!(0x00, "Create Character", 0x68, C),
    p!(0x01, "Disconnect", 0x05, C),
    p!(0x02, "Walk Request", 0x07, C),
    p!(0x03, "Client Talk", 0, B),
    p!(0x04, "Request God mode (God client)", 0x02, B),
    p!(0x05, "Attack", 0x05, C),
    p!(0x06, "Double Click", 0x05, C),
    p!(0x07, "Pick Up Item", 0x07, C),
    p!(0x08, "Drop Item", 0x0e, C),
    p!(0x09, "Single Click", 0x05, C),
    p!(0x0A, "Edit (God client)", 0x0b, B),
    p!(0x0B, "Damage Visualization", 0x07, S),
    p!(0x0C, "Edit tiledata (God client)", 0, B),
    p!(0x0D, "Edit NPC data (God client)", 0x03, B),
    p!(0x0E, "Edit template data (God client)", 0x01, B),
    p!(0x0F, _, 0x3d),
    p!(0x10, "Edit hue data (God client)", 0xd7, B),
    p!(0x11, "Character Status", 0, S),
    p!(0x12, "Perform Action", 0, C),
    p!(0x13, "Client Equip Item", 0x0a, C),
    p!(0x14, "Change tile Z (God client)", 0x06, B),
    p!(0x15, "Follow", 0x09, B),
    p!(0x16, "Health status bar update (0x16)", 0x01, S),
    p!(0x17, "Health status bar update (KR)", 0, S),
    p!(0x18, "Add script (God client)", 0, B),
    p!(0x19, "Edit NPC speech (God client)", 0, B),
    p!(0x1A, "Update Item", 0, S),
    p!(0x1B, "Enter World", 0x25, S),
    p!(0x1C, "Server Talk", 0, S),
    p!(0x1D, "Delete Object", 0x05, S),
    p!(0x1E, "Animate?", 0x04, B),
    p!(0x1F, "Explode?", 0x08, B),
    p!(0x20, "Update Player", 0x13, S),
    p!(0x21, "Deny Walk", 0x08, S),
    p!(0x22, "Confirm Walk", 0x03, B),
    p!(0x23, "Drag Animation", 0x1a, S),
    p!(0x24, "Open Container", 0x07, S),
    p!(0x25, "Update Contained Item", 0x14, S),
    p!(0x26, "Kick client (God client)", 0x05, B),
    p!(0x27, "Deny Move Item", 0x02, S),
    p!(0x28, "End dragging item", 0x05, S),
    p!(0x29, "Drop Item Accepted", 0x01, S),
    p!(0x2A, "Blood mode", 0x05, S),
    p!(0x2B, "Toggle God mode (God client)", 0x02, B),
    p!(0x2C, "Death Screen", 0x02, B),
    p!(0x2D, "Mobile Attributes", 0x11, S),
    p!(0x2E, "Server Equip Item", 0x0f, S),
    p!(0x2F, "Combat Notification", 0x0a, S),
    p!(0x30, "Attack ok", 0x05, S),
    p!(0x31, "Attack end", 0x01, S),
    p!(0x32, "Toggle hack mover (God client)", 0x02, B),
    p!(0x33, "Pause Control", 0x02, S),
    p!(0x34, "Status Request", 0x0a, C),
    p!(0x35, "Resource type (God client)", 0x28d, B),
    p!(0x36, "Resource tile data (God client)", 0, B),
    p!(0x37, "Move object (God client)", 0x08, B),
    p!(0x38, "Pathfinding", 0x07, S),
    p!(0x39, "Remove group (God client)", 0x09, B),
    p!(0x3A, "Update Skills", 0, B),
    p!(0x3B, "Vendor Buy Reply", 0, B),
    p!(0x3C, "Update Contained Items", 0, S),
    p!(0x3D, "Ship (God client)", 0x02, B),
    p!(0x3E, "Versions (God client)", 0x25, B),
    p!(0x3F, "Update Statics (God Client)", 0, B),
    p!(0x40, "Update terrains (God client)", 0xc9, B),
    p!(0x41, "Update terrains (God client)", 0, B),
    p!(0x42, "Update art (God client)", 0, B),
    p!(0x43, "Update animation (God client)", 0x229, B),
    p!(0x44, "Update hues (God client)", 0x2c9, B),
    p!(0x45, "Version OK (God client)", 0x05, B),
    p!(0x46, "New art (God client)", 0, B),
    p!(0x47, "New terrain (God client)", 0x0b, B),
    p!(0x48, "New animation (God client)", 0x49, B),
    p!(0x49, "New hues (God client)", 0x5d, B),
    p!(0x4A, "Destroy art (God client)", 0x05, B),
    p!(0x4B, "Check version (God client)", 0x09, B),
    p!(0x4C, "Script names (God client)", 0, B),
    p!(0x4D, "Edit script (God client)", 0, B),
    p!(0x4E, "Personal Light Level", 0x06, S),
    p!(0x4F, "Global Light Level", 0x02, S),
    p!(0x50, "Board header", 0, B),
    p!(0x51, "Board message", 0, B),
    p!(0x52, "Post board message", 0, B),
    p!(0x53, "Error Code", 0x02, S),
    p!(0x54, "Sound Effect", 0x0c, S),
    p!(0x55, "Login Complete", 0x01, S),
    p!(0x56, "Map Data", 0x0b, B),
    p!(0x57, "Update regions (God client)", 0x6e, B),
    p!(0x58, "New region (God client)", 0x6a, B),
    p!(0x59, "New content FX (God client)", 0, B),
    p!(0x5A, "Update content FX (God client)", 0, B),
    p!(0x5B, "Set Time", 0x04, S),
    p!(0x5C, "Restart Version", 0x02, B),
    p!(0x5D, "Select Character", 0x49, C),
    p!(0x5E, "Server list (God client)", 0, B),
    p!(0x5F, "Add server (God client)", 0x31, B),
    p!(0x60, "Remove server (God client)", 0x05, B),
    p!(0x61, "Destroy static (God client)", 0x09, B),
    p!(0x62, "Move static (God client)", 0x0f, B),
    p!(0x63, "Area load (God client)", 0x0d, B),
    p!(0x64, "Area load request (God client)", 0x01, B),
    p!(0x65, "Set Weather", 0x04, S),
    p!(0x66, "Book Page Data", 0, B),
    p!(0x67, "Simped? (God client)", 0x15, B),
    p!(0x68, "Script attach (God client)", 0, B),
    p!(0x69, "Friends (God client)", 0, B),
    p!(0x6A, "Notify friend (God client)", 0x03, B),
    p!(0x6B, "Key use (God client)", 0x09, B),
    p!(0x6C, "Target Data", 0x13, B),
    p!(0x6D, "Play Music", 0x03, S),
    p!(0x6E, "Character Animation", 0x0e, S),
    p!(0x6F, "Secure Trading", 0, B),
    p!(0x70, "Graphic Effect", 0x1c, S),
    p!(0x71, "Bulletin Board Data", 0, B),
    p!(0x72, "War Mode", 0x05, B),
    p!(0x73, "Ping", 0x02, B),
    p!(0x74, "Vendor Buy List", 0, S),
    p!(0x75, "Rename Character", 0x23, C),
    p!(0x76, "New Subserver", 0x10, S),
    p!(0x77, "Update Character", 0x11, S),
    p!(0x78, "Update Object", 0, S),
    p!(0x79, "Resource query (God client)", 0x09, B),
    p!(0x7A, "Resource data (God client)", 0, B),
    p!(0x7B, "Sequence?", 0x02, S),
    p!(0x7C, "Open Menu Gump", 0, S),
    p!(0x7D, "Menu Choice", 0x0d, C),
    p!(0x7E, "God view query (God client)", 0x02, B),
    p!(0x7F, "God view data (God client)", 0, B),
    p!(0x80, "First Login", 0x3e, C),
    p!(0x81, "Change character", 0, S),
    p!(0x82, "Login Error", 0x02, S),
    p!(0x83, "Delete Character", 0x27, C),
    p!(0x84, "Change password", 0x45, B),
    p!(0x85, "Character List Notification", 0x02, S),
    p!(0x86, "Resend Character List", 0, S),
    p!(0x87, "Send resources (God client)", 0, B),
    p!(0x88, "Open Paperdoll", 0x42, S),
    p!(0x89, "Corpse Equipment", 0, S),
    p!(0x8A, "Trigger edit (God client)", 0, B),
    p!(0x8B, "Display sign gump", 0, S),
    p!(0x8C, "Relay Server", 0x0b, S),
    p!(0x8D, "UO3D create character", 0, C),
    p!(0x8E, "Move character (God client)", 0, B),
    p!(0x8F, _, 0),
    p!(0x90, "Display Map", 0x13, S),
    p!(0x91, "Second Login", 0x41, C),
    p!(0x92, "Update multi data (God client)", 0, B),
    p!(0x93, "Open Book", 0x63, S),
    p!(0x94, "Update skills data (God client)", 0, B),
    p!(0x95, "Dye Data", 0x09, B),
    p!(0x96, "Game central monitor (God client)", 0, B),
    p!(0x97, "Move Player", 0x02, S),
    p!(0x98, "All Names (3D Client Only)", 0, B),
    p!(0x99, "Multi Placement", 0x1a, B),
    p!(0x9A, "ASCII Prompt", 0, B),
    p!(0x9B, "Help Request", 0x102, C),
    p!(0x9C, "Assistant request (God client)", 0x135, B),
    p!(0x9D, "GM single (God client)", 0x33, B),
    p!(0x9E, "Vendor Sell List", 0, S),
    p!(0x9F, "Vendor Sell Reply", 0, C),
    p!(0xA0, "Select Server", 0x03, C),
    p!(0xA1, "Update Hitpoints", 0x09, S),
    p!(0xA2, "Update Mana", 0x09, S),
    p!(0xA3, "Update Stamina", 0x09, S),
    p!(0xA4, "System Information", 0x95, C),
    p!(0xA5, "Open URL", 0, S),
    p!(0xA6, "Tip Window", 0, S),
    p!(0xA7, "Request Tip", 0x04, C),
    p!(0xA8, "Server List", 0, S),
    p!(0xA9, "Character List", 0, S),
    p!(0xAA, "Attack Reply", 0x05, S),
    p!(0xAB, "Text Entry Dialog", 0, S),
    p!(0xAC, "Text Entry Dialog Reply", 0, C),
    p!(0xAD, "Unicode Client Talk", 0, C),
    p!(0xAE, "Unicode Server Talk", 0, S),
    p!(0xAF, "Display Death", 0x0d, S),
    p!(0xB0, "Open Gump", 0, S),
    p!(0xB1, "Gump Choice", 0, C),
    p!(0xB2, "Chat Data", 0, B),
    p!(0xB3, "Chat Text ?", 0, S),
    p!(0xB4, "Target object list (God client)", 0, B),
    p!(0xB5, "Open Chat Window", 0x40, B),
    p!(0xB6, "Popup Help Request", 0x09, C),
    p!(0xB7, "Popup Help Data", 0, S),
    p!(0xB8, "Character Profile", 0, B),
    p!(0xB9, "Enable locked client features", 0x03, S),
    p!(0xBA, "Display Quest Arrow", 0x06, S),
    p!(0xBB, "Account ID ?", 0x09, C),
    p!(0xBC, "Season", 0x03, S),
    p!(0xBD, "Client Version", 0, B),
    p!(0xBE, "Assist Version", 0, B),
    p!(0xBF, "Extended Command", 0, B),
    p!(0xC0, "Graphical Effect", 0x24, S),
    p!(0xC1, "Display cliloc String", 0, S),
    p!(0xC2, "Unicode prompt", 0, B),
    p!(0xC3, _, 0),
    p!(0xC4, "Semivisible", 0x06, B),
    p!(0xC5, "Invalid map (God client)", 0xcb, B),
    p!(0xC6, "Invalid map enable", 0x01, S),
    p!(0xC7, "Graphical Effect", 0x31, S),
    p!(0xC8, "Client View Range", 0x02, B),
    p!(0xC9, "Trip time", 0x06, B),
    p!(0xCA, "UTrip time", 0x06, B),
    p!(0xCB, _, 0x07),
    p!(0xCC, "Localized Text Plus String", 0, S),
    p!(0xCD, _, 0x01),
    p!(0xCE, _, 0),
    p!(0xCF, _, 0x4e),
    p!(0xD0, _, 0),
    p!(0xD1, "Logout", 0x02, S),
    p!(0xD2, "Update Character (New)", 0x19, S),
    p!(0xD3, "Update Object (New)", 0, S),
    p!(0xD4, "Open Book (New)", 0, B),
    p!(0xD5, _, 0),
    p!(0xD6, "Mega cliloc", 0, B),
    p!(0xD7, "+AoS command", 0, C),
    p!(0xD8, "Custom house", 0, S),
    p!(0xD9, "+Metrics", 0x10c, C),
    p!(0xDA, "Mahjong game command", 0, B),
    p!(0xDB, "Character transfer log", 0, S),
    p!(0xDC, "OPL Info Packet", 9, S),
    p!(0xDD, "Compressed Gump", 0, S),
    p!(0xDE, "Update characters combatants", 0, S),
    p!(0xDF, "Buff/Debuff", 0, S),
    p!(0xE0, "Bug Report KR", 0, C),
    p!(0xE1, "Client Type KR/SA", 0x09, C),
    p!(0xE2, "New Character Animation", 0xa, S),
    p!(0xE3, "KR Encryption Responce", 0x4d, S),
    p!(0xE4, _, 0),
    p!(0xE5, _, 0),
    p!(0xE6, _, 0),
    p!(0xE7, _, 0),
    p!(0xE8, _, 0),
    p!(0xE9, _, 0),
    p!(0xEA, _, 0),
    p!(0xEB, _, 0),
    p!(0xEC, "Equip Macro", 0, C),
    p!(0xED, "Unequip item macro", 0, C),
    p!(0xEE, _, 0),
    p!(0xEF, "KR/2D Client Login/Seed", 0x15, C),
    p!(0xF0, "Krrios client special", 0, B),
    p!(0xF1, "Client-Server Time Synchronization Request", 0, C),
    p!(0xF2, "Client-Server Time Synchronization Response", 0, S),
    p!(0xF3, "Update Item (SA)", 0x18, S),
    p!(0xF4, _, 0),
    p!(0xF5, "Display New Map", 0x15, S),
    p!(0xF6, "Boat moving", 0, S),
    p!(0xF7, "Packets (0xF3) list", 0, S),
    p!(0xF8, "Character Creation (7.0.16.0)", 0x6a, C),
    p!(0xF9, _, 0),
    p!(0xFA, "Open UO Store", 0x01, C),
    p!(0xFB, "Show Public House Content", 0x02, C),
    p!(0xFC, "CrossUO messages", 0, B),
    p!(0xFD, _, 0),
    p!(0xFE, "Razor Handshake", 0x8, S),
    p!(0xFF, _, 0),
];

/// Client version as (major, minor, rev, patch), e.g. 7.0.9.0
pub type ClientVersion = (u16, u16, u16, u16);

pub fn parse_version(s: &str) -> Option<ClientVersion> {
    let parts: Vec<u16> = s.split('.').filter_map(|p| p.parse().ok()).collect();
    if parts.is_empty() || parts.len() > 4 {
        return None;
    }
    let mut v = [0u16; 4];
    v[..parts.len()].copy_from_slice(&parts);
    Some((v[0], v[1], v[2], v[3]))
}

fn ver_ge(a: ClientVersion, b: ClientVersion) -> bool {
    a >= b
}

/// Mirrors CPacketManager::ConfigureClientVersion (PacketManager.cpp:453).
/// Returns per-id size overrides to apply on top of the static table.
pub fn apply_version_overrides(table: &mut [PacketInfo; 0x100], ver: ClientVersion) {
    let var = 0u16;
    if ver_ge(ver, (5, 0, 0, 0)) {
        table[0x0B].size = 0x07;
        table[0x16].size = var;
        table[0x31].size = var;
    } else {
        table[0x0B].size = 0x10A;
        table[0x16].size = 0x01;
        table[0x31].size = 0x01;
    }
    if ver_ge(ver, (5, 0, 9, 0)) {
        table[0xE1].size = var;
    } else {
        table[0xE1].size = 0x09;
    }
    if ver_ge(ver, (6, 0, 1, 3)) {
        table[0xE3].size = var;
        table[0xE6].size = 0x05;
        table[0xE7].size = 0x0C;
        table[0xE8].size = 0x0D;
        table[0xE9].size = 0x4B;
        table[0xEA].size = 0x03;
    } else {
        table[0xE3].size = 0x4D;
        table[0xE6].size = var;
        table[0xE7].size = var;
        table[0xE8].size = var;
        table[0xE9].size = var;
        table[0xEA].size = var;
    }
    if ver_ge(ver, (6, 0, 1, 7)) {
        table[0x08].size = 0x0F;
        table[0x25].size = 0x15;
    } else {
        table[0x08].size = 0x0E;
        table[0x25].size = 0x14;
    }
    if ver == (6, 0, 6, 0) {
        table[0xEE].size = 0x2000;
        table[0xEF].size = 0x2000;
        table[0xF1].size = 0x09;
    } else {
        table[0xEE].size = var;
        table[0xEF].size = 0x15;
        table[0xF1].size = var;
    }
    if ver_ge(ver, (6, 0, 14, 2)) {
        table[0xB9].size = 0x05;
    } else {
        table[0xB9].size = 0x03;
    }
    if ver_ge(ver, (7, 0, 0, 0)) {
        table[0xEE].size = 0x2000;
        table[0xEF].size = 0x2000;
    } else {
        table[0xEE].size = var;
        table[0xEF].size = 0x15;
    }
    if ver_ge(ver, (7, 0, 9, 0)) {
        table[0x24].size = 0x09;
        table[0x99].size = 0x1E;
        table[0xBA].size = 0x0A;
        table[0xF3].size = 0x1A;
        table[0xF1].size = 0x09;
        table[0xF2].size = 0x19;
    } else {
        table[0x24].size = 0x07;
        table[0x99].size = 0x1A;
        table[0xBA].size = 0x06;
        table[0xF3].size = 0x18;
        table[0xF1].size = var;
        table[0xF2].size = var;
    }
    if ver_ge(ver, (7, 0, 18, 0)) {
        table[0x00].size = 0x6A;
    }
}

use std::sync::RwLock;

static TABLE: RwLock<Option<Box<[PacketInfo; 0x100]>>> = RwLock::new(None);

/// Set (or replace) the global version-patched table. Called at startup with
/// the configured fallback and again once the client announces its real
/// version via the 0xEF connection packet (ConnectionManager.cpp:163-181).
pub fn set_table(ver: ClientVersion) {
    let mut t = Box::new(PACKETS);
    apply_version_overrides(&mut t, ver);
    // The 0xEF connection packet has a fixed wire layout (id + seed + 16-byte
    // version = 0x15); the client-version override would set it to 0x2000
    // (PacketManager.cpp:573-575) and it would never frame.
    t[0xEF].size = 0x15;
    *TABLE.write().unwrap() = Some(t);
}

pub fn table() -> std::sync::RwLockReadGuard<'static, Option<Box<[PacketInfo; 0x100]>>> {
    TABLE.read().unwrap()
}

/// Extract the client version from a 0xEF "KR/2D Client Login/Seed" packet:
/// [0]=0xEF, [1..5]=seed, then major/minor/rev/prot as BE u32s. A prot byte
/// value >= 'a' is treated as 0 (ConnectionManager.cpp:173-176).
/// Returns None if the packet is not a well-formed 0xEF.
pub fn version_from_ef_packet(pkt: &[u8]) -> Option<ClientVersion> {
    if pkt.len() < 21 || pkt[0] != 0xEF {
        return None;
    }
    let u32be = |o: usize| -> u16 {
        let v = u32::from_be_bytes([pkt[o], pkt[o + 1], pkt[o + 2], pkt[o + 3]]);
        v.min(u16::MAX as u32) as u16
    };
    let major = u32be(5);
    let minor = u32be(9);
    let rev = u32be(13);
    let prot_raw = u32::from_be_bytes([pkt[17], pkt[18], pkt[19], pkt[20]]);
    let prot = if prot_raw >= 'a' as u32 {
        0
    } else {
        prot_raw.min(9) as u16
    };
    Some((major, minor, rev, prot))
}

pub fn info(id: u8) -> &'static PacketInfo {
    &PACKETS[id as usize]
}

pub fn is_relevant_to(info: &PacketInfo, to_server: bool) -> bool {
    match info.dir {
        Direction::Both => true,
        Direction::ClientToServer => to_server,
        Direction::ServerToClient => !to_server,
    }
}
