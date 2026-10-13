// Recognition of special bytes/markers introduced (or handled specially) by
// the crossuo client, so dumps expose them explicitly instead of hiding them
// in raw hex. Mirrors:
//   - ConnectionManager.cpp login seed (SeedOverride)
//   - PacketManager.cpp 0x03 talk acknowledge (SystemCheckOverride)
//   - PacketManager.cpp 0xF0 Krrios/ServUO bundle + 0xFE Razor negotiation
//     (assistant-feature mask decode, xuocore/enumlist.h ASSISTANT_FEATURE)
//   - Packets.cpp CPacketSelectCharacter         (0x5D clientFlag)
//
// Annotation rules:
//   - every value is shown as a hex byte list
//   - when there is an interpretation, it follows in parentheses
//     e.g. "SEED: 0x7f 0x00 0x00 0x01 (0x7f000001 = 127.0.0.1)"

pub fn hex_bytes(data: &[u8]) -> String {
    data.iter()
        .map(|b| format!("0x{:02x}", b))
        .collect::<Vec<_>>()
        .join(" ")
}

pub fn u32be(data: &[u8]) -> u32 {
    ((data[0] as u32) << 24) | ((data[1] as u32) << 16) | ((data[2] as u32) << 8) | data[3] as u32
}

pub fn u64be(data: &[u8]) -> u64 {
    let mut v: u64 = 0;
    for b in data {
        v = (v << 8) | *b as u64;
    }
    v
}

/// "SEED: 0x7f 0x00 0x00 0x01 (0x7f000001 = 127.0.0.1)"
/// The seed is normally the server IP big-endian; crossuo sends the fixed
/// patchSeed (0x000148aa) instead.
pub fn seed_line(seed: &[u8]) -> String {
    if seed.len() != 4 {
        return format!("SEED: {} ({} bytes)", hex_bytes(seed), seed.len());
    }
    let v = u32be(seed);
    let octets = [(v >> 24) as u8, (v >> 16) as u8, (v >> 8) as u8, v as u8];
    let ip = format!("{}.{}.{}.{}", octets[0], octets[1], octets[2], octets[3]);
    format!("SEED: {} (0x{:08x} = server ip {})", hex_bytes(seed), v, ip)
}

/// Client version fields of the 0xEF connection packet (offset 5..21).
pub fn ef_version_lines(pkt: &[u8]) -> Vec<String> {
    if pkt.len() < 21 {
        return vec![];
    }
    let mut out = vec![seed_line(&pkt[1..5])];
    let major = u32be(&pkt[5..9]);
    let minor = u32be(&pkt[9..13]);
    let rev = u32be(&pkt[13..17]);
    let prot_raw = u32be(&pkt[17..21]);
    let prot = if prot_raw >= 'a' as u32 { 0 } else { prot_raw };
    out.push(format!(
        "CLIENT VERSION: {}.{}.{}.{} (major=0x{:08x} minor=0x{:08x} rev=0x{:08x} prot=0x{:08x})",
        major, minor, rev, prot, major, minor, rev, prot_raw
    ));
    out
}

/// Talk acknowledge reply (client->server 0x03 packets sent after a server
/// 0x03 SYSTEM message): dump header, size and payload so the payload can be
/// pasted straight into `SystemCheckOverride=` in crossuo.cfg
pub fn talk_ack_line(pkt: &[u8]) -> String {
    let size = ((pkt[1] as usize) << 8) | pkt[2] as usize;
    format!(
        "SYSTEM CHECK: size 0x{:04x} {} bytes: {}",
        size,
        pkt.len(),
        hex_bytes(&pkt[3..])
    )
}

/// Dispatch the special-field annotations for a packet.
pub fn fields(pkt: &[u8], to_server: bool) -> Vec<String> {
    match pkt[0] {
        0xEF if to_server => ef_version_lines(pkt),
        0x5D if to_server => select_character_lines(pkt),
        0x03 if to_server => vec![talk_ack_line(pkt)],
        0xFB if to_server => show_public_house_content_lines(pkt),
        0xF0 => krrios_special_lines(pkt, to_server),
        0xFE if !to_server => razor_negotiate_lines(pkt),
        _ => vec![],
    }
}

/// 0xFB Show Public House Content annotation (Packets.cpp
/// CPacketShowPublicHouseContent): byte 1 is the show flag
/// (mirrors ClassicUO Send_ShowPublicHouseContent, 7.0.79.6+).
fn show_public_house_content_lines(pkt: &[u8]) -> Vec<String> {
    if pkt.len() < 2 {
        return vec![];
    }
    vec![format!("  [1] show: {}", pkt[1] != 0)]
}

// ---------------------------------------------------------------------------
// 0xF0 Krrios/ServUO protocol-extension bundle + 0xFE Razor negotiation
// (PacketManager.cpp KrriosClientSpecial/RazorNegotiate; ServUO
// Scripts/Services/Assistants.cs, Scripts/Misc/ProtocolExtensions.cs:
// the negotiate handshake is an 0xF0 bundle with subcommand 0xFE, the Razor
// reply the same bundle with subcommand 0xFF: F0 00 04 FF)
// ---------------------------------------------------------------------------
const ASF_FILTER_WEATHER: u64 = 1 << 0;
const ASF_FILTER_LIGHT: u64 = 1 << 1;
const ASF_SMART_TARGET: u64 = 1 << 2;
const ASF_RANGED_TARGET: u64 = 1 << 3;
const ASF_AUTO_OPEN_DOORS: u64 = 1 << 4;
const ASF_DEQUIP_ON_CAST: u64 = 1 << 5;
const ASF_AUTO_POTION_EQUIP: u64 = 1 << 6;
const ASF_POISONED_CHECKS: u64 = 1 << 7;
const ASF_LOOPED_MACROS: u64 = 1 << 8;
const ASF_USE_ONCE_AGENT: u64 = 1 << 9;
const ASF_RESTOCK_AGENT: u64 = 1 << 10;
const ASF_SELL_AGENT: u64 = 1 << 11;
const ASF_BUY_AGENT: u64 = 1 << 12;
const ASF_POTION_HOTKEYS: u64 = 1 << 13;
const ASF_RANDOM_TARGETS: u64 = 1 << 14;
const ASF_CLOSEST_TARGETS: u64 = 1 << 15;
const ASF_OVERHEAD_HEALTH: u64 = 1 << 16;
const ASF_AUTOLOOT_AGENT: u64 = 1 << 17; // AssistUO only
const ASF_BONE_CUTTER_AGENT: u64 = 1 << 18; // AssistUO only
const ASF_JSCRIPT_MACROS: u64 = 1 << 19; // AssistUO only
const ASF_AUTO_REMOUNT: u64 = 1 << 20; // AssistUO only

/// Decode the assistant-negotiation disallowed-features mask
/// (ASSISTANT_FEATURE in xuocore/enumlist.h; set bit == disallowed).
fn assistant_feature_names(mask: u64) -> String {
    const FLAGS: [(u64, &str); 21] = [
        (ASF_FILTER_WEATHER, "filter-weather"),
        (ASF_FILTER_LIGHT, "filter-light"),
        (ASF_SMART_TARGET, "smart-target"),
        (ASF_RANGED_TARGET, "ranged-target"),
        (ASF_AUTO_OPEN_DOORS, "auto-open-doors"),
        (ASF_DEQUIP_ON_CAST, "dequip-on-cast"),
        (ASF_AUTO_POTION_EQUIP, "auto-potion-equip"),
        (ASF_POISONED_CHECKS, "poisoned-checks"),
        (ASF_LOOPED_MACROS, "looped-macros"),
        (ASF_USE_ONCE_AGENT, "use-once-agent"),
        (ASF_RESTOCK_AGENT, "restock-agent"),
        (ASF_SELL_AGENT, "sell-agent"),
        (ASF_BUY_AGENT, "buy-agent"),
        (ASF_POTION_HOTKEYS, "potion-hotkeys"),
        (ASF_RANDOM_TARGETS, "random-targets"),
        (ASF_CLOSEST_TARGETS, "closest-targets"),
        (ASF_OVERHEAD_HEALTH, "overhead-health"),
        (ASF_AUTOLOOT_AGENT, "autoloot-agent"),
        (ASF_BONE_CUTTER_AGENT, "bone-cutter-agent"),
        (ASF_JSCRIPT_MACROS, "jscript-macros"),
        (ASF_AUTO_REMOUNT, "auto-remount"),
    ];
    if mask == 0 {
        return "none (all assistant features allowed)".to_string();
    }
    let mut names: Vec<&str> = Vec::new();
    let mut unknown = mask;
    for (bit, name) in FLAGS {
        if mask & bit != 0 {
            names.push(name);
            unknown &= !bit;
        }
    }
    let mut out = names.join("|");
    if unknown != 0 {
        out.push_str(&format!("|unknown:0x{:x}", unknown));
    }
    out
}

fn krrios_sub_name(sub: u8, to_server: bool) -> &'static str {
    match (sub, to_server) {
        (0x00, false) => "krrios probe accepted",
        (0x01, false) => "party member positions",
        (0x02, false) => "guild member positions",
        (0x03, false) => "runebook contents",
        (0x04, false) => "guardline data",
        (0xFE, true) => "razor/krrios probe (answer with the ACK below)",
        (0xFE, false) => "assistant negotiation (disallowed features mask)",
        (0xFF, true) => "razor ACK (client identifies as Razor)",
        (0xFF, false) => "razor ACK",
        _ => "unknown",
    }
}

/// 0xF0 bundle: [0]=0xF0, [1..3]=BE size, [3]=subcommand, [4..]=payload.
pub fn krrios_special_lines(pkt: &[u8], to_server: bool) -> Vec<String> {
    if pkt.len() < 4 || pkt[0] != 0xF0 {
        return vec![];
    }
    let size = ((pkt[1] as usize) << 8) | pkt[2] as usize;
    let sub = pkt[3];
    let mut out = vec![format!(
        "SUB: 0x{:02x} = {} (size 0x{:04x})",
        sub,
        krrios_sub_name(sub, to_server),
        size
    )];
    match sub {
        // assistant negotiation: 8-byte disallowed-features mask
        0xFE if !to_server => {
            if pkt.len() >= 12 {
                let mask = u64be(&pkt[4..12]);
                out.push(format!(
                    "DISALLOWED FEATURES: 0x{:016x} = {}",
                    mask,
                    assistant_feature_names(mask)
                ));
            } else {
                out.push(format!(
                    "DISALLOWED FEATURES: truncated ({} bytes, need 8)",
                    pkt.len() - 4
                ));
            }
        }
        // razor ACK: no payload beyond the subcommand byte
        0xFF if to_server => {
            if pkt.len() > 4 {
                out.push(format!("PAYLOAD: {}", hex_bytes(&pkt[4..])));
            }
        }
        // party/guild member positions: serial, x, y, (map when locations set)
        0x01 | 0x02 if !to_server => {
            if sub == 0x02 && pkt.len() > 4 {
                out.push(format!("  [4] locations: {}", pkt[4] != 0));
            }
            let locations = sub == 0x02 && pkt.len() > 4 && pkt[4] != 0;
            let entry = if locations { 11 } else { 7 };
            let start = if sub == 0x02 { 5 } else { 4 };
            let count = (pkt.len().saturating_sub(start)) / entry;
            out.push(format!("  ENTRIES: {} ({} bytes each)", count, entry));
        }
        _ => {}
    }
    out
}

/// Standalone 0xFE Razor negotiation (legacy RunUO style, server -> client):
/// payload is the 8-byte disallowed-features mask. Note: ServUO sends the
/// handshake as an 0xF0 bundle (see above), never as a bare 0xFE.
pub fn razor_negotiate_lines(pkt: &[u8]) -> Vec<String> {
    let mask_len = std::cmp::min(8, pkt.len().saturating_sub(1));
    if mask_len == 0 {
        return vec![];
    }
    let mut mask_bytes = [0u8; 8];
    mask_bytes[..mask_len].copy_from_slice(&pkt[1..1 + mask_len]);
    let mask = u64be(&mask_bytes);
    let truncated = if mask_len < 8 { " (truncated)" } else { "" };
    vec![format!(
        "DISALLOWED FEATURES: 0x{:016x}{} = {}",
        mask,
        truncated,
        assistant_feature_names(mask)
    )]
}

// ---------------------------------------------------------------------------
// CLIENT_FLAG / GetClientTypeString (xuocore/enumlist.h:177, Config.cpp:186)
// ---------------------------------------------------------------------------
const CF_T2A: u32 = 0x00; // The Second Age (base, no bits)
const CF_RE: u32 = 0x01; // Renaissance
const CF_TD: u32 = 0x02; // Third Dawn
const CF_LBR: u32 = 0x04; // Lord Blackthorn's Revenge
const CF_AOS: u32 = 0x08; // Age Of Shadows
const CF_SE: u32 = 0x10; // Samurai Empire
const CF_SA: u32 = 0x20; // Stygian Abyss
const CF_UO3D: u32 = 0x40; // UO3D
const CF_RESERVED: u32 = 0x80; // Reserved
const CF_3D: u32 = 0x100; // 3D client

/// Decode the 0x5D client-flag mask into expansion names, following
/// `enum CLIENT_FLAG` and `GetClientTypeString` (xuocore/enumlist.h:177,
/// Config.cpp:186). CF_T2A == 0x00 (no bits = plain T2A client); each set bit
/// adds one expansion. Unknown bits are reported instead of swallowed.
fn client_flag_names(flag: u32) -> String {
    const FLAGS: [(u32, &str); 9] = [
        (CF_RE, "re"),     // Renaissance
        (CF_TD, "td"),     // Third Dawn
        (CF_LBR, "lbr"),   // Lord Blackthorn's Revenge
        (CF_AOS, "aos"),   // Age Of Shadows
        (CF_SE, "se"),     // Samurai Empire
        (CF_SA, "sa"),     // Stygian Abyss
        (CF_UO3D, "uo3d"), // UO3D
        (CF_RESERVED, "reserved"),
        (CF_3D, "3d"), // 3D client
    ];
    let mut names: Vec<&str> = Vec::new();
    if flag & CF_T2A as u32 == 0 && flag < CF_RE as u32 {
        names.push("t2a"); // The Second Age: CLIENT_FLAG base, no bits set
    }
    let mut unknown = flag;
    for (bit, name) in FLAGS {
        if flag & bit != 0 {
            names.push(name);
            unknown &= !bit;
        }
    }
    let mut out = names.join("|");
    if unknown != 0 {
        if !out.is_empty() {
            out.push('|');
        }
        out.push_str(&format!("unknown:0x{:x}", unknown));
    }
    out
}

/// 0x5D Select Character field annotations.
/// Layout (Packets.cpp CPacketSelectCharacter):
///   [0]       0x5D
///   [1..5]    pattern 0xEDEDEDED
///   [5..35]   character name
///   [35..37]  (unused)
///   [37..41]  client flag
///   [41..65]  (unused)
///   [65..69]  character slot index
///   [69..73]  client IP
pub fn select_character_lines(pkt: &[u8]) -> Vec<String> {
    if pkt.len() < 73 || pkt[0] != 0x5D {
        return vec![];
    }
    let mut out = vec![];
    let pattern = u32be(&pkt[1..5]);
    if pattern == 0xEDEDEDED {
        out.push("PATTERN: 0xEDEDEDED (filler pattern, expected)".to_string());
    } else {
        out.push(format!(
            "PATTERN: {} (0x{:08x}, unexpected, expected 0xEDEDEDED)",
            hex_bytes(&pkt[1..5]),
            pattern
        ));
    }

    let name_end = pkt[5..35]
        .iter()
        .position(|&b| b == 0)
        .map(|i| 5 + i)
        .unwrap_or(35);
    let name = String::from_utf8_lossy(&pkt[5..name_end]);
    out.push(format!(
        "CHARACTER NAME: {} ({})",
        hex_bytes(&pkt[5..name_end]),
        name
    ));

    let flag = u32be(&pkt[37..41]);
    out.push(format!(
        "CLIENT FLAG: {} (0x{:08x} = {})",
        hex_bytes(&pkt[37..41]),
        flag,
        client_flag_names(flag)
    ));

    let slot = u32be(&pkt[65..69]);
    out.push(format!(
        "CHARACTER SLOT: {} (0x{:08x})",
        hex_bytes(&pkt[65..69]),
        slot
    ));

    let ip_v = u32be(&pkt[69..73]);
    out.push(format!(
        "CLIENT IP: {} (0x{:08x} = {}.{}.{}.{})",
        hex_bytes(&pkt[69..73]),
        ip_v,
        (ip_v >> 24) as u8,
        (ip_v >> 16) as u8,
        (ip_v >> 8) as u8,
        ip_v as u8
    ));
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn client_flag_decoding() {
        assert_eq!(client_flag_names(0x3F), "re|td|lbr|aos|se|sa");
        assert_eq!(client_flag_names(0x7F), "re|td|lbr|aos|se|sa|uo3d");
        assert_eq!(client_flag_names(0x00), "t2a");
        assert_eq!(
            client_flag_names(0xFFFFFFFF),
            "re|td|lbr|aos|se|sa|uo3d|reserved|3d|unknown:0xfffffe00"
        );
        assert_eq!(client_flag_names(0xF0000003), "re|td|unknown:0xf0000000");
        assert_eq!(assistant_feature_names(0), "none (all assistant features allowed)");
        assert_eq!(
            assistant_feature_names(ASF_SMART_TARGET | ASF_LOOPED_MACROS),
            "smart-target|looped-macros"
        );
        assert_eq!(
            assistant_feature_names(u64::MAX),
            "filter-weather|filter-light|smart-target|ranged-target|auto-open-doors|dequip-on-cast|auto-potion-equip|poisoned-checks|looped-macros|use-once-agent|restock-agent|sell-agent|buy-agent|potion-hotkeys|random-targets|closest-targets|overhead-health|autoloot-agent|bone-cutter-agent|jscript-macros|auto-remount|unknown:0xffffffffffe00000"
        );
        assert_eq!(client_flag_names(0x100), "3d");
    }

    #[test]
    fn select_character_lines_ok() {
        let mut pkt = vec![0u8; 73];
        pkt[0] = 0x5D;
        pkt[1..5].copy_from_slice(&0xEDEDEDEDu32.to_be_bytes());
        pkt[37..41].copy_from_slice(&0xFFFFFFFFu32.to_be_bytes());
        let lines = select_character_lines(&pkt);
        assert!(lines.iter().any(|l| l.contains("0xffffffff")));
    }
}

#[cfg(test)]
mod negotiate_tests {
    use super::*;

    #[test]
    fn krrios_special_handshake() {
        // ServUO BeginHandshake: F0 00 0C FE + 8-byte mask (smart LT + looped macros)
        let pkt = [0xF0, 0x00, 0x0C, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04];
        let lines = fields(&pkt, false);
        assert!(lines[0].contains("SUB: 0xfe"), "{:?}", lines);
        assert!(lines[0].contains("assistant negotiation"), "{:?}", lines);
        assert!(lines[1].contains("smart-target|looped-macros"), "{:?}", lines);
    }

    #[test]
    fn krrios_special_razor_ack() {
        let pkt = [0xF0, 0x00, 0x04, 0xFF];
        let lines = fields(&pkt, true);
        assert!(lines[0].contains("razor ACK"), "{:?}", lines);
    }

    #[test]
    fn krrios_special_party_positions() {
        let pkt = [0xF0, 0x00, 0x0B, 0x01, 0,0,0,1, 0,1, 0,0];
        let lines = fields(&pkt, false);
        assert!(lines[0].contains("party member positions"), "{:?}", lines);
        assert!(lines[1].contains("1 (7 bytes each)"), "{:?}", lines);
    }

    #[test]
    fn standalone_negotiate() {
        let pkt = [0xFE, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x08];
        let lines = fields(&pkt, false);
        assert!(lines[0].contains("ranged-target"), "{:?}", lines);
    }
}
