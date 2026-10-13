// Optional JSON expansion of packets ("--json").
//
// A decoder gets the full raw packet and returns JSON (compact, single line
// so diffs stay stable) or None to fall back to hex-only output.
//
// Extending: implement `Decoder` and register it in `default_registry()`.
// Only wire parsing lives here; nothing writes to the stream, so the proxy
// stays transparent regardless of what decoders do.
use crate::packets::{self, PacketInfo};

pub trait Decoder: Send + Sync {
    fn decode(&self, pkt: &[u8], info: &PacketInfo) -> Option<String>;
}

fn u16be(p: &[u8], off: usize) -> u16 {
    ((p[off] as u16) << 8) | p[off + 1] as u16
}
fn u32be(p: &[u8], off: usize) -> u32 {
    ((p[off] as u32) << 24) | ((p[off + 1] as u32) << 16) | ((p[off + 2] as u32) << 8) | p[off + 3] as u32
}
fn i16be(p: &[u8], off: usize) -> i16 {
    u16be(p, off) as i16
}
fn i8v(p: &[u8], off: usize) -> i8 {
    p[off] as i8
}

fn json_str(s: &str) -> String {
    let mut out = String::with_capacity(s.len() + 2);
    out.push('"');
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if (c as u32) < 0x20 => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out.push('"');
    out
}

fn ascii_z(p: &[u8], start: usize, max: usize) -> String {
    let end = p[start..(start + max).min(p.len())]
        .iter()
        .position(|&b| b == 0)
        .map(|i| start + i)
        .unwrap_or((start + max).min(p.len()));
    String::from_utf8_lossy(&p[start..end]).into_owned()
}

pub struct Registry {
    decoders: Vec<(u8, Box<dyn Decoder>)>,
}

impl Registry {
    pub fn new() -> Self {
        Self { decoders: Vec::new() }
    }

    pub fn add(&mut self, id: u8, d: Box<dyn Decoder>) {
        self.decoders.push((id, d));
    }

    pub fn decode(&self, pkt: &[u8]) -> Option<String> {
        let info = packets::info(pkt[0]);
        self.decoders
            .iter()
            .find(|(id, _)| *id == pkt[0])
            .and_then(|(_, d)| d.decode(pkt, info))
    }
}

// ---------------------------------------------------------------------------
// Built-in decoders
// ---------------------------------------------------------------------------

struct RelayServer; // 0x8C
impl Decoder for RelayServer {
    fn decode(&self, p: &[u8], _i: &PacketInfo) -> Option<String> {
        if p.len() < 11 {
            return None;
        }
        let ip = format!("{}.{}.{}.{}", p[1], p[2], p[3], p[4]);
        let port = u16be(p, 5);
        let seed = u32be(p, 7);
        Some(format!(
            r#"{{"ip":"{}","port":{},"seed":{:#04x}}}"#,
            ip, port, seed
        ))
    }
}

struct ClientVersion; // 0xBD
impl Decoder for ClientVersion {
    fn decode(&self, p: &[u8], _i: &PacketInfo) -> Option<String> {
        if p.len() < 4 {
            return None;
        }
        let len = u16be(p, 1) as usize;
        if p.len() < len {
            return None;
        }
        let ver = String::from_utf8_lossy(&p[3..len - 1]);
        Some(format!(r#"{{"version":{}}}"#, json_str(&ver)))
    }
}

struct ServerList; // 0xA8
impl Decoder for ServerList {
    fn decode(&self, p: &[u8], _i: &PacketInfo) -> Option<String> {
        if p.len() < 6 {
            return None;
        }
        let count = u16be(p, 4) as usize;
        let mut servers = Vec::new();
        let mut off = 6usize;
        for _ in 0..count {
            if off + 40 > p.len() {
                break;
            }
            let name = ascii_z(p, off + 2, 32);
            servers.push(format!(
                r#"{{"index":{},"name":{}}}"#,
                u16be(p, off),
                json_str(&name)
            ));
            off += 40;
        }
        Some(format!(
            r#"{{"flags":{:#04x},"count":{},"servers":[{}]}}"#,
            p[3],
            servers.len(),
            servers.join(",")
        ))
    }
}

struct CharacterList; // 0xA9
impl Decoder for CharacterList {
    fn decode(&self, p: &[u8], _i: &PacketInfo) -> Option<String> {
        if p.len() < 4 {
            return None;
        }
        let count = p[3] as usize;
        let mut chars = Vec::new();
        let mut off = 4usize;
        for _ in 0..count {
            if off + 60 > p.len() {
                break;
            }
            let name = ascii_z(p, off, 30);
            chars.push(json_str(&name));
            off += 60;
        }
        Some(format!(r#"{{"count":{},"characters":[{}]}}"#, chars.len(), chars.join(",")))
    }
}

struct Talk; // 0x1C ascii server talk
impl Decoder for Talk {
    fn decode(&self, p: &[u8], _i: &PacketInfo) -> Option<String> {
        if p.len() < 44 {
            return None;
        }
        let serial = u32be(p, 2);
        let graphic = u16be(p, 6);
        let kind = p[8];
        let hue = u16be(p, 9);
        let font = u16be(p, 11);
        let name = ascii_z(p, 14, 30);
        let text = String::from_utf8_lossy(&p[44..]).into_owned();
        Some(format!(
            r#"{{"serial":{:#010x},"graphic":{:#06x},"type":{},"hue":{},"font":{},"name":{},"text":{}}}"#,
            serial,
            graphic,
            kind,
            hue,
            font,
            json_str(&name),
            json_str(&text)
        ))
    }
}

struct DeleteObject; // 0x1D
impl Decoder for DeleteObject {
    fn decode(&self, p: &[u8], _i: &PacketInfo) -> Option<String> {
        if p.len() < 5 {
            return None;
        }
        Some(format!(r#"{{"serial":{:#010x}}}"#, u32be(p, 1)))
    }
}

struct UpdateHitpoints; // 0xA1/0xA2/0xA3 share layout
impl Decoder for UpdateHitpoints {
    fn decode(&self, p: &[u8], _i: &PacketInfo) -> Option<String> {
        if p.len() < 9 {
            return None;
        }
        Some(format!(
            r#"{{"serial":{:#010x},"max":{},"cur":{}}}"#,
            u32be(p, 1),
            i16be(p, 5),
            i16be(p, 7)
        ))
    }
}

struct MobileStatus; // 0x11 variable character status
impl Decoder for MobileStatus {
    fn decode(&self, p: &[u8], _i: &PacketInfo) -> Option<String> {
        if p.len() < 43 {
            return None;
        }
        let serial = u32be(p, 1);
        let name = ascii_z(p, 5, 30);
        let hp = i16be(p, 37);
        let max_hp = i16be(p, 39);
        Some(format!(
            r#"{{"serial":{:#010x},"name":{},"hp":{},"max_hp":{}}}"#,
            serial,
            json_str(&name),
            hp,
            max_hp
        ))
    }
}

struct Damage; // 0x0B
impl Decoder for Damage {
    fn decode(&self, p: &[u8], _i: &PacketInfo) -> Option<String> {
        if p.len() < 7 {
            return None;
        }
        Some(format!(
            r#"{{"target":{:#010x},"damage":{}}}"#,
            u32be(p, 1),
            u16be(p, 5)
        ))
    }
}

struct MoveRej; // 0x21 deny walk
impl Decoder for MoveRej {
    fn decode(&self, p: &[u8], _i: &PacketInfo) -> Option<String> {
        if p.len() < 8 {
            return None;
        }
        Some(format!(
            r#"{{"seq":{},"x":{},"y":{},"dir":{},"z":{}}}"#,
            p[1],
            u16be(p, 2),
            u16be(p, 4),
            p[6] & 0x7f,
            i8v(p, 7)
        ))
    }
}

struct MoveAck; // 0x22 confirm walk
impl Decoder for MoveAck {
    fn decode(&self, p: &[u8], _i: &PacketInfo) -> Option<String> {
        if p.len() < 3 {
            return None;
        }
        Some(format!(r#"{{"seq":{},"noto":{:#04x}}}"#, p[1], p[2]))
    }
}

struct Ping; // 0x73
impl Decoder for Ping {
    fn decode(&self, p: &[u8], _i: &PacketInfo) -> Option<String> {
        if p.len() < 2 {
            return None;
        }
        Some(format!(r#"{{"echo":{}}}"#, p[1]))
    }
}

pub fn default_registry() -> Registry {
    let mut r = Registry::new();
    r.add(0x8C, Box::new(RelayServer));
    r.add(0xBD, Box::new(ClientVersion));
    r.add(0xA8, Box::new(ServerList));
    r.add(0xA9, Box::new(CharacterList));
    r.add(0x1C, Box::new(Talk));
    r.add(0x1D, Box::new(DeleteObject));
    r.add(0xA1, Box::new(UpdateHitpoints));
    r.add(0xA2, Box::new(UpdateHitpoints));
    r.add(0xA3, Box::new(UpdateHitpoints));
    r.add(0x11, Box::new(MobileStatus));
    r.add(0x0B, Box::new(Damage));
    r.add(0x21, Box::new(MoveRej));
    r.add(0x22, Box::new(MoveAck));
    r.add(0x73, Box::new(Ping));
    r
}
