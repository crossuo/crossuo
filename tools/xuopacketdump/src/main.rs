// xuopacketdump - a transparent TCP proxy for Ultima Online clients that dumps
// every packet exchanged between client and server into a diff-friendly
// text file.
//
// Wire logic is mirrored from crossuo:
//   - packet table / version size overrides: src/Managers/PacketManager.cpp
//   - framing (fixed sizes; variable = BE u16 at offset 1): src/Wisp/WispPacketMessage.cpp
//
// Login flow handling:
//   1. STAGE LOGIN  - first connection (login server). The client sends a
//      4-byte seed before the first framed packet; it is dumped as a SEED
//      block. Stage ends when the server sends 0x8C "Relay Server".
//   2. STAGE GAME   - the client then disconnects and connects to the relay
//      address. The proxy rewrites the relay IP/port inside the passing 0x8C
//      packet to point back at this proxy, so the game connection is captured
//      too; it is forwarded to the REAL relay address (a loopback relay IP is
//      translated to the --connect host) and flagged STAGE GAME.
//
// Old client encryption (uo1998 1.25.x and friends): every post-seed C->S byte
// on both the login and the game socket is encrypted with the seed-keyed login
// stream cipher (the game socket re-seeds the same stream with the relay seed
// carried by 0x8C). The proxy decrypts a local copy for framing/dumping (see
// crypt.rs) while the original encrypted bytes are forwarded untouched.

mod crypt;
mod decode;
mod filter;
mod format;
mod framer;
mod huffman;
mod packets;
mod special;

use std::io::{Read, Write};
use std::net::{IpAddr, Ipv4Addr, TcpListener, TcpStream, ToSocketAddrs};
use std::sync::{Arc, Mutex};
use std::thread;

use filter::Filter;
use framer::Framer;

#[derive(Clone, Copy, PartialEq)]
enum Stage {
    Login,
    Game,
}

impl Stage {
    fn name(self) -> &'static str {
        match self {
            Stage::Login => "LOGIN",
            Stage::Game => "GAME",
        }
    }
}

struct Config {
    listen: String,
    connect: String,
    output: String,
    filter: Filter,
    json: bool,
    timestamps: bool,
    client_version: packets::ClientVersion,
    rewrite_relay: bool,
    /// first local port used for per-server game proxies; one port per server
    /// entry in the 0xA8 list is allocated starting here (--game-port)
    game_port_base: u16,
    connect_ip_for_client: Ipv4Addr,
    /// login crypt variant for C->S framing (crypt::CipherKind)
    cipher: crypt::CipherKind,
    /// g_Config.Seed equivalent (0x1357 unless the client cfg sets UseKeys)
    crypt_seed: u32,
}

/// A game-server proxy: the local port the client is told to dial and the real
/// (host, port) the connection is forwarded to.
#[derive(Clone)]
struct GameProxy {
    local_port: u16,
    target: String,
}

struct Shared {
    out: Mutex<std::fs::File>,
    cfg: Config,
    decoders: decode::Registry,
    /// game proxies registered per local port (populated when the 0xA8 server
    /// list is seen, one entry per advertised server)
    proxies: Mutex<std::collections::HashMap<u16, String>>,
    /// game seed from the last 0x8C, reused for every game connection
    game_seed: Mutex<Option<[u8; 4]>>,
}

fn write_marker(shared: &Shared, text: &str) {
    let mut out = shared.out.lock().unwrap();
    let _ = format::marker(&mut *out, text);
    let _ = out.flush();
}

fn write_seed(shared: &Shared, stage: Stage, seed: &[u8]) {
    let mut out = shared.out.lock().unwrap();
    let _ = format::marker(&mut *out, &format!("SEED STAGE={} ({} bytes)", stage.name(), seed.len()));
    let _ = format::hexdump(&mut *out, seed);
    let _ = writeln!(&mut *out, "{}", special::seed_line(seed));
    let _ = out.flush();
}

fn direction_label(to_server: bool) -> &'static str {
    if to_server {
        "CLIENT->SERVER"
    } else {
        "SERVER->CLIENT"
    }
}

fn write_packet(shared: &Shared, to_server: bool, pkt: &[u8]) -> bool {
    let id = pkt[0];
    let info = packets::info(id);
    if !packets::is_relevant_to(info, to_server) {
        // still forwarded untouched, just not dumped
        return true;
    }
    if shared.cfg.filter.is_ignored(id) {
        return true;
    }

    let mut out = shared.out.lock().unwrap();
    let _ = format::header(
        &mut *out,
        direction_label(to_server),
        id,
        info.name,
        pkt.len(),
    );
    if shared.cfg.timestamps {
        let ts = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map(|d| d.as_millis())
            .unwrap_or(0);
        let _ = writeln!(&mut *out, "TS {}", ts);
    }
    let _ = format::hexdump(&mut *out, pkt);
    // special-field annotations (seed, version, client flag, talk ack, ...)
    for line in special::fields(pkt, to_server) {
        let _ = writeln!(&mut *out, "{}", line);
    }
    if shared.cfg.json {
        if let Some(json) = shared.decoders.decode(pkt) {
            let _ = writeln!(&mut *out, "JSON {}", json);
        }
    }
    let _ = out.flush();
    true
}

/// Handle connection-level interception logic (login stage only). Returns
/// true when the proxy altered the packet bytes (server list rewrite).
fn on_packet_from_server(shared: &Arc<Shared>, stage: Stage, pkt: &mut Vec<u8>) -> bool {
    if stage != Stage::Login || !shared.cfg.rewrite_relay {
        return false;
    }
    if pkt[0] == 0xA8 && pkt.len() >= 5 {
        rewrite_server_list(shared, pkt);
        return true;
    }
    if pkt[0] == 0x8C && pkt.len() >= 11 {
        // 0x8C Relay Server: [0]=0x8C [1..5]=ip [5..7]=port [7..11]=seed
        // Keep the seed for the game connection; rewrite the address to a
        // proxy port allocated for this relay target.
        *shared.game_seed.lock().unwrap() = Some([pkt[7], pkt[8], pkt[9], pkt[10]]);
        let real_ip = Ipv4Addr::new(pkt[1], pkt[2], pkt[3], pkt[4]);
        let real_port = u16::from_be_bytes([pkt[5], pkt[6]]);
        let target = translate_relay(shared, real_ip, real_port);
        let local_port = ensure_proxy(shared, &target);
        let ip = shared.cfg.connect_ip_for_client.octets();
        pkt[1..5].copy_from_slice(&ip);
        pkt[5..7].copy_from_slice(&local_port.to_be_bytes());
        let mut out = shared.out.lock().unwrap();
        let _ = writeln!(
            &mut *out,
            "REWRITE 0x8C relay {}:{} -> {}:{} (game proxy -> {})",
            real_ip, real_port, shared.cfg.connect_ip_for_client, local_port, target
        );
        let _ = out.flush();
        return true;
    }
    false
}

/// A loopback/unspecified relay IP means "the same machine as the login
/// server": rewrite it to the --connect host (remote clients can never reach
/// the server's loopback address).
fn translate_relay(shared: &Arc<Shared>, ip: Ipv4Addr, port: u16) -> String {
    if ip.is_loopback() || ip.is_unspecified() {
        // The shard relays to its own loopback port (e.g. 127.0.0.1:2599),
        // which remote clients can never reach. Mirror crossuo's
        // IgnoreRelay=yes path: reuse the address AND port the login
        // connection used (that server also serves the game stage there).
        shared.cfg.connect.clone()
    } else {
        format!("{}:{}", ip, port)
    }
}

/// Return the local proxy port for `target`, allocating the next free port
/// (starting at --game-port) and spawning a listener on first use.
fn ensure_proxy(shared: &Arc<Shared>, target: &str) -> u16 {
    {
        let proxies = shared.proxies.lock().unwrap();
        if let Some((port, _)) = proxies.iter().find(|(_, t)| t.as_str() == target) {
            return *port;
        }
    }
    let mut port = shared.cfg.game_port_base;
    loop {
        let addrs = format!("{}:{}", shared.cfg.connect_ip_for_client, port);
        match TcpListener::bind(&addrs) {
            Ok(listener) => {
                shared
                    .proxies
                    .lock()
                    .unwrap()
                    .insert(port, target.to_string());
                let shared = Arc::clone(shared);
                let target = target.to_string();
                thread::spawn(move || {
                    for stream in listener.incoming() {
                        let stream = match stream {
                            Ok(s) => s,
                            Err(_) => continue,
                        };
                        let shared = Arc::clone(&shared);
                        let target = target.clone();
                        thread::spawn(move || handle_game_connection(shared, stream, target));
                    }
                });
                return port;
            }
            Err(_) => {
                port = port.wrapping_add(1);
                if port == 0 || port == shared.cfg.game_port_base {
                    eprintln!("xuopacketdump: no free game proxy port");
                    return shared.cfg.game_port_base;
                }
            }
        }
    }
}

/// Rewrite every server entry in an 0xA8 list to point at its own local proxy
/// port. Layout (CServerList::ParsePacket): [0]=0xA8 [1..3]=len(BE)
/// [3..5]=count(BE) then per entry:
/// Entries do not carry a game port, so the authoritative rewrite is the 0x8C
/// relay packet (which has ip:port and is what the client dials). The list is
/// logged with each server's ip so the dump shows the real topology.
/// 0xA8 server list, verified byte-for-byte against the uo1998 shard:
///   [0]=0xA8  [1..3]=length(BE)  [3]=0x5D (unknown/flags)
///   [4..6]=count(BE)  [6]=0x00 (unused)
///   records from [6], each 40 bytes:
///     id(2 BE) + name(32, NUL padded) + fullPercent(1) + timezone(1) + ip(4, BE)
///     (verified: 2 entries consume exactly the 86-byte packet)
/// Entries carry no game port; that arrives with the 0x8C relay packet.
fn rewrite_server_list(shared: &Arc<Shared>, pkt: &mut Vec<u8>) {
    const REC: usize = 40;
    const FIRST: usize = 6;
    if pkt.len() < FIRST {
        return;
    }
    let count = u16::from_be_bytes([pkt[4], pkt[5]]) as usize;
    let mut pos = FIRST;
    let mut notes: Vec<String> = Vec::new();
    for i in 0..count {
        if pos + REC > pkt.len() {
            notes.push(format!("  [{}] truncated record", i));
            break;
        }
        let id = u16::from_be_bytes([pkt[pos], pkt[pos + 1]]);
        let name_end = pos + 2 + 32;
        let name = String::from_utf8_lossy(
            &pkt[pos + 2..name_end]
                .iter()
                .copied()
                .take_while(|&b| b != 0)
                .collect::<Vec<u8>>(),
        )
        .to_string();
        let full = pkt[name_end];
        let tz = pkt[name_end + 1];
        let ip_off = name_end + 2;
        let ip =
            Ipv4Addr::new(pkt[ip_off], pkt[ip_off + 1], pkt[ip_off + 2], pkt[ip_off + 3]);
        notes.push(format!(
            "  [{}] id={} '{}' full={}% tz={} ip={} (game port comes with 0x8C)",
            i, id, name, full, tz, ip
        ));
        pos += REC;
    }
    let mut out = shared.out.lock().unwrap();
    let _ = writeln!(
        &mut *out,
        "SERVER LIST 0xA8 ({} entries) - game proxies follow the 0x8C relay",
        count
    );
    for n in notes {
        let _ = writeln!(&mut *out, "{}", n);
    }
    let _ = out.flush();
}

/// Game-stage connection handler for a per-server proxy: connect to the real
/// server and pump packets like any other connection.
fn handle_game_connection(shared: Arc<Shared>, client: TcpStream, target: String) {
    let peer = client.peer_addr().map(|a| a.to_string()).unwrap_or_default();
    write_marker(&shared, &format!("CONNECT STAGE=GAME from {} -> {}", peer, target));
    let server = match connect_bounded(&target, 5) {
        Ok(s) => s,
        Err(e) => {
            write_marker(&shared, &format!("CONNECT FAILED to {}: {}", target, e));
            eprintln!("xuopacketdump: cannot connect game server {}: {}", target, e);
            return;
        }
    };
    let _ = client.set_nodelay(true);
    let _ = server.set_nodelay(true);
    let (c2s_client, c2s_server, s2c_client, s2c_server) = match (
        client.try_clone(),
        server.try_clone(),
        client.try_clone(),
        server.try_clone(),
    ) {
        (Ok(a), Ok(b), Ok(c), Ok(d)) => (a, b, c, d),
        _ => return,
    };
    // Same teardown guarantee as the login path: when either leg ends, close
    // the upstream game session instead of leaving it half-open.
    let kill_s1 = server.try_clone();
    let kill_s2 = server.try_clone();
    let kill_c1 = client.try_clone();
    let kill_c2 = client.try_clone();
    let sh1 = Arc::clone(&shared);
    let sh2 = Arc::clone(&shared);
    let t1 = thread::spawn(move || {
        pump(sh1, c2s_client, c2s_server, true, true, Stage::Game);
        if let Ok(k) = kill_s1 {
            let _ = k.shutdown(std::net::Shutdown::Both);
        }
    });
    let t2 = thread::spawn(move || {
        pump(sh2, s2c_server, s2c_client, false, false, Stage::Game);
        if let Ok(k) = kill_c2 {
            let _ = k.shutdown(std::net::Shutdown::Both);
        }
    });
    let _ = t1.join();
    let _ = t2.join();
    let _ = (kill_c1, kill_s2);
    write_marker(&shared, &format!("DISCONNECT STAGE=GAME ({})", target));
}

/// Resolve + connect with a bounded timeout so dead hosts cannot stall.
fn connect_bounded(target: &str, secs: u64) -> std::io::Result<TcpStream> {
    let mut addrs = target.to_socket_addrs()?;
    let addr = addrs
        .find(|a| a.is_ipv4())
        .ok_or_else(|| std::io::Error::new(std::io::ErrorKind::NotFound, "no ipv4 address"))?;
    TcpStream::connect_timeout(&addr, std::time::Duration::from_secs(secs))
}

/// Local decryptor for the C->S stream. Holds the cipher variant until the
/// seed is observed, then arms the keyed stream. The forwarded bytes stay
/// encrypted; only the framing copy is decrypted.
struct StreamCrypt {
    kind: crypt::CipherKind,
    state: Option<crypt::LoginCrypt>,
    armed: bool,
}

impl StreamCrypt {
    fn new(kind: crypt::CipherKind) -> Self {
        StreamCrypt {
            kind,
            state: None,
            armed: false,
        }
    }

    fn arm(&mut self, shared: &Shared, seed: &[u8; 4]) {
        if self.kind == crypt::CipherKind::None || self.armed {
            return;
        }
        let k = crypt::derive_keys_for(shared.cfg.client_version);
        self.state = Some(crypt::LoginCrypt::new(seed, shared.cfg.crypt_seed, k));
        self.armed = true;
        write_marker(
            shared,
            &format!(
                "LOGIN CRYPT {:?} armed (seed {:02x} {:02x} {:02x} {:02x}, client version {}.{}.{}.{})",
                self.kind,
                seed[0],
                seed[1],
                seed[2],
                seed[3],
                shared.cfg.client_version.0,
                shared.cfg.client_version.1,
                shared.cfg.client_version.2,
                shared.cfg.client_version.3
            ),
        );
    }

    fn plain(&mut self, data: &[u8]) -> Vec<u8> {
        match (&mut self.state, self.armed) {
            (Some(state), true) => {
                let mut buf = data.to_vec();
                state.xform(&mut buf, self.kind);
                buf
            }
            _ => data.to_vec(),
        }
    }
}

fn pump(
    shared: Arc<Shared>,
    mut reader: TcpStream,
    mut writer: TcpStream,
    to_server: bool,
    expect_seed: bool,
    stage: Stage,
) {
    // The wire must carry the bytes the peer actually wrote. We therefore:
    //   1. append every received chunk to `raw` (the forward buffer, indexed
    //      by logical stream offset via `base`),
    //   2. decrypt the same chunk into the framer for dump/size purposes,
    //   3. when a popped packet is mutated (0x8C relay rewrite) patch the
    //      matching bytes inside `raw`,
    //   4. forward `raw` up to the current end.
    // Any other design risks sending plaintext (or duplicated bytes) upstream,
    // which desyncs the server's cipher state and kills the session.
    let mut crypt = StreamCrypt::new(if to_server {
        shared.cfg.cipher
    } else {
        crypt::CipherKind::None
    });
    let mut framer = Framer::new();
    let mut seeded = !expect_seed;
    let mut buf = [0u8; 0x1000];

    // raw bytes not yet written to the peer, with the logical offset of [0]
    let mut raw: Vec<u8> = Vec::new();
    let mut base: usize = 0;
    // logical offset of the first byte currently held inside the framer
    let mut frame_at: usize = 0;
    let mut stream_in: usize = 0;
    let mut stream_out: usize = 0;
    // The game socket's S->C stream is Huffman-compressed by the server and
    // decompressed by the client before framing (CSocket::Decompression).
    // Decompress a copy so packets frame; the raw bytes still go to the wire.
    let decompress = !to_server && stage == Stage::Game;
    let mut huff = huffman::HuffmanDecompressor::new();
    let mut first_bytes: Vec<u8> = Vec::new();
    let mut framer_pending_tail: Vec<u8> = Vec::new();

    loop {
        let n = match reader.read(&mut buf) {
            Ok(0) => break,
            Ok(n) => n,
            Err(_) => break,
        };
        let chunk = &buf[..n];
        if first_bytes.len() < 32 {
            first_bytes.extend(chunk.iter().take(32 - first_bytes.len()));
        }
        stream_in += n;

        let mut decrypted: Vec<u8>;
        let mut flushed_seed = false;

        if !seeded {
            if to_server && raw.is_empty() {
                raw.extend_from_slice(chunk);
                if raw.len() < 4 {
                    flush_raw(&mut writer, &mut raw, &mut base, usize::MAX);
                    continue;
                }
                if stage == Stage::Login && raw[0] == 0xEF {
                    // plaintext modern handshake
                    seeded = true;
                    decrypted = chunk.to_vec();
                } else {
                    let seed: [u8; 4] = raw[..4].try_into().unwrap();
                    write_seed(&shared, stage, &seed);
                    crypt.arm(&shared, &seed);
                    // the seed is clear text; everything after it is ciphered
                    let rest = raw[4..].to_vec();
                    decrypted = crypt.plain(&rest);
                    // forward the seed immediately, keep the rest for patching
                    if writer.write_all(&raw[..4]).is_err() {
                        break;
                    }
                    stream_out += 4;
                    base += 4;
                    raw.drain(..4);
                    frame_at = base;
                    seeded = true;
                    // fall through so the packets in `decrypted` are popped
                    // and dumped (they would otherwise be skipped entirely)
                    framer.push(&decrypted);
                    decrypted.clear();
                    flushed_seed = true;
                }
            } else {
                seeded = true;
                raw.extend_from_slice(chunk);
                decrypted = chunk.to_vec();
            }
        } else {
            raw.extend_from_slice(chunk);
            if decompress {
                decrypted = Vec::new();
                huff.decompress(chunk, &mut decrypted);
            } else {
                decrypted = crypt.plain(chunk);
            }
        }

        framer.push(&decrypted);

        // Pop packets: dump them, patch raw when mutated.
        while let Some(mut pkt) = framer.next_packet() {
            let pkt_at = frame_at;
            frame_at += pkt.len();
            if to_server {
                if let Some(ver) = packets::version_from_ef_packet(&pkt) {
                    packets::set_table(ver);
                    write_marker(
                        &shared,
                        &format!(
                            "CLIENT VERSION {}.{}.{}.{} deduced from 0xEF",
                            ver.0, ver.1, ver.2, ver.3
                        ),
                    );
                }
            } else {
                let before = pkt.clone();
                on_packet_from_server(&shared, stage, &mut pkt);
                if pkt != before {
                    let lo = pkt_at.wrapping_sub(base);
                    if lo + pkt.len() <= raw.len() {
                        raw[lo..lo + pkt.len()].copy_from_slice(&pkt);
                    } else {
                        write_marker(&shared, "WARN relay patch skipped (offset out of range)");
                    }
                }
                if pkt[0] == 0x8C {
                    write_marker(&shared, "RELAY 0x8C received - next connection is STAGE GAME");
                }
                if pkt[0] == 0x82 {
                    write_marker(&shared, "LOGIN ERROR 0x82 (login denied)");
                }
                if pkt[0] == 0x55 {
                    write_marker(&shared, "LOGIN COMPLETE 0x55");
                }
            }
            write_packet(&shared, to_server, &pkt);
        }

        // Forward everything received so far.
        if writer.write_all(&raw).is_err() {
            break;
        }
        stream_out += raw.len();
        base += raw.len();
        raw.clear();
    }

    if !raw.is_empty() {
        let _ = writer.write_all(&raw);
    }
    let _ = &framer_pending_tail;
    let tail: Vec<u8> = framer.pending().to_vec();
    write_marker(
        &shared,
        &format!(
            "STATS {} read={} forwarded={} framed={}",
            if to_server { "C->S" } else { "S->C" },
            stream_in,
            stream_out,
            frame_at
        ),
    );
    write_marker(
        &shared,
        &format!(
            "FIRST {} bytes: {:02X?}{}",
            if to_server { "C->S" } else { "S->C" },
            first_bytes,
            if decompress { " (raw, pre-decompression)" } else { "" }
        ),
    );
    write_marker(
        &shared,
        &format!(
            "UNFRAMED {} tail ({} bytes): {:02X?}",
            if to_server { "C->S" } else { "S->C" },
            tail.len(),
            &tail[..tail.len().min(32)]
        ),
    );
    let _ = writer.shutdown(std::net::Shutdown::Write);
}

fn flush_raw(w: &mut TcpStream, raw: &mut Vec<u8>, base: &mut usize, _max: usize) {
    if raw.is_empty() {
        return;
    }
    if w.write_all(raw).is_ok() {
        *base += raw.len();
        raw.clear();
    }
}

fn handle_connection(shared: Arc<Shared>, client: TcpStream, stage: Stage) {
    let peer = client.peer_addr().map(|a| a.to_string()).unwrap_or_default();
    write_marker(&shared, &format!("CONNECT STAGE={} from {}", stage.name(), peer));

    // Only the login listener uses handle_connection now; game connections are
    // accepted by their per-server proxy listeners (handle_game_connection).
    let server = match connect_bounded(&shared.cfg.connect, 5) {
        Ok(s) => s,
        Err(e) => {
            write_marker(&shared, &format!("CONNECT FAILED to {}: {}", shared.cfg.connect, e));
            eprintln!("xuopacketdump: cannot connect to {}: {}", shared.cfg.connect, e);
            return;
        }
    };
    let _ = client.set_nodelay(true);
    let _ = server.set_nodelay(true);

    let c2s_client = match client.try_clone() {
        Ok(s) => s,
        Err(_) => return,
    };
    let c2s_server = match server.try_clone() {
        Ok(s) => s,
        Err(_) => return,
    };
    let s2c_client = match client.try_clone() {
        Ok(s) => s,
        Err(_) => return,
    };
    let s2c_server = match server.try_clone() {
        Ok(s) => s,
        Err(_) => return,
    };

    // Each pump gets a shutdown handle so that when EITHER leg finishes the
    // whole connection is torn down. Without this the upstream socket stayed
    // half-open and the shard kept the account marked "in use" (0x82 02)
    // until its own timeout, blocking every subsequent login.
    let kill_s1 = server.try_clone();
    let kill_c1 = client.try_clone();
    let kill_s2 = server.try_clone();
    let kill_c2 = client.try_clone();

    let sh1 = Arc::clone(&shared);
    let sh2 = Arc::clone(&shared);
    let t1 = thread::spawn(move || {
        pump(sh1, c2s_client, c2s_server, true, true, stage);
        // client leg ended: drop the upstream session immediately
        if let Ok(k) = kill_s1 {
            let _ = k.shutdown(std::net::Shutdown::Both);
        }
    });
    let t2 = thread::spawn(move || {
        pump(sh2, s2c_server, s2c_client, false, false, stage);
        // server leg ended: make sure the client leg unwinds too
        if let Ok(k) = kill_c2 {
            let _ = k.shutdown(std::net::Shutdown::Both);
        }
    });
    let _ = t1.join();
    let _ = t2.join();
    let _ = (kill_c1, kill_s2);

    write_marker(&shared, &format!("DISCONNECT STAGE={}", stage.name()));
}

const USAGE: &str = "\
xuopacketdump - UO client/server packet dump proxy

USAGE:
  xuopacketdump --listen <ip:port> --connect <host:port> [options]

OPTIONS:
  -l, --listen <ip:port>     local proxy address the client connects to (login server)
  -c, --connect <host:port>  real login server address to forward to
  -o, --output <file>        dump file (default: xuopacketdump.txt)
  -f, --filter <file>        filter config: packet IDs to ignore (denoise)
  -j, --json                 decode registered packets as JSON lines
  -t, --timestamps           add a TS epoch line per packet
  -v, --client-version <ver> fallback client version for pre-6.0.6.0 clients (which send no
                             0xEF packet); also selects the login crypt keys, so it MUST match
                             the client (e.g. 1.25.32 for the uo1998 client)
      --game-port <port>     first local port for per-server game proxies (default 2595);
                             one port per server entry is allocated from here
      --no-rewrite-relay     do not rewrite 0x8C to route the game connection
                             back through the proxy
      --crypt <mode>         login crypt variant: auto | old | 1.25.36 | bfish | none
                             (default auto = derived from --client-version; old shards
                             like uo1998 use \"old\")
      --crypt-seed <hex>     login crypt seed key (default 0x1357; only changed when the
                             client cfg carries UseKeys)
  -h, --help

NOTES:
  - The 0x8C relay packet is rewritten back to this proxy and the game-stage
    connection is forwarded to the real relay address reported by the shard
    (loopback relay IPs are translated to the --connect host).
  - Pre-6.0.6.0 clients encrypt every post-seed C->S byte; the proxy decrypts
    a local copy for dumping and forwards the encrypted bytes unchanged.
";

/// Ok(None) = auto (derive from client version); Ok(Some(k)) = explicit.
fn parse_cipher(s: &str) -> Result<Option<crypt::CipherKind>, ()> {
    match s.to_ascii_lowercase().as_str() {
        "auto" => Ok(None),
        "old" => Ok(Some(crypt::CipherKind::Old)),
        "1.25.36" | "12536" | "1_25_36" => Ok(Some(crypt::CipherKind::V12536)),
        "bfish" | "bfish-stream" => Ok(Some(crypt::CipherKind::BFish)),
        "none" | "off" => Ok(Some(crypt::CipherKind::None)),
        _ => Err(()),
    }
}

fn parse_args() -> Option<Config> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    if args.iter().any(|a| a == "-h" || a == "--help") || args.is_empty() {
        println!("{}", USAGE);
        return None;
    }
    let mut listen = None;
    let mut connect = None;
    let mut output = "xuopacketdump.txt".to_string();
    let mut filter = Filter::default();
    let mut json = false;
    let mut timestamps = false;
    let mut client_version = (7, 0, 9, 0);
    let mut rewrite_relay = true;
    let mut game_port_base = 2595u16;
    let mut cipher: Option<crypt::CipherKind> = None; // None = auto (from version)
    let mut explicit_cipher = false;
    let mut crypt_seed = 0x1357u32;

    let mut i = 0;
    while i < args.len() {
        let a = &args[i];
        let val = |i: &mut usize| -> Option<String> {
            *i += 1;
            args.get(*i).cloned()
        };
        match a.as_str() {
            "-l" | "--listen" => listen = val(&mut i),
            "-c" | "--connect" => connect = val(&mut i),
            "-o" | "--output" => {
                if let Some(v) = val(&mut i) {
                    output = v;
                }
            }
            "-f" | "--filter" => {
                if let Some(v) = val(&mut i) {
                    match Filter::from_file(&v) {
                        Ok(f) => filter = f,
                        Err(e) => {
                            eprintln!("xuopacketdump: filter error: {}", e);
                            return None;
                        }
                    }
                }
            }
            "-j" | "--json" => json = true,
            "-t" | "--timestamps" => timestamps = true,
            "-v" | "--client-version" => {
                if let Some(v) = val(&mut i) {
                    match packets::parse_version(&v) {
                        Some(ver) => client_version = ver,
                        None => {
                            eprintln!("xuopacketdump: bad version '{}'", v);
                            return None;
                        }
                    }
                }
            }
            "--game-port" => {
                if let Some(v) = val(&mut i) {
                    game_port_base = v.parse().unwrap_or(2595);
                }
            }
            "--no-rewrite-relay" => rewrite_relay = false,
            "--crypt" => {
                let v = match val(&mut i) {
                    Some(v) => v,
                    None => {
                        eprintln!("xuopacketdump: --crypt requires a value");
                        return None;
                    }
                };
                match parse_cipher(&v) {
                    Ok(m) => {
                        cipher = m;
                        explicit_cipher = true;
                    }
                    Err(()) => {
                        eprintln!("xuopacketdump: bad --crypt value (auto|old|1.25.36|bfish|none)");
                        return None;
                    }
                }
            }
            "--crypt-seed" => {
                if let Some(v) = val(&mut i) {
                    match u32::from_str_radix(v.trim_start_matches("0x"), 16) {
                        Ok(s) => crypt_seed = s,
                        Err(_) => {
                            eprintln!("xuopacketdump: bad --crypt-seed '{}'", v);
                            return None;
                        }
                    }
                }
            }
            other => {
                eprintln!("xuopacketdump: unknown option '{}'", other);
                return None;
            }
        }
        i += 1;
    }

    let listen = listen?;
    let connect = connect?;

    // auto: mirror SetClientCrypt for the old stream ciphers; 2.0.3+ clients
    // (blowfish/twofish) are not supported and fall back to no crypt.
    let v32 = ((client_version.0 as u32) << 24)
        | ((client_version.1 as u32) << 16)
        | ((client_version.2 as u32) << 8)
        | client_version.3 as u32;
    let resolved = if explicit_cipher {
        cipher.expect("explicitly requested cipher")
    } else {
        match crypt::cipher_for_version(v32) {
            Some(k) => k,
            None => {
                eprintln!(
                    "xuopacketdump: warning: client version {}.{}.{}.{} uses blowfish/twofish login \
                     crypt which is not supported; C->S packets will be dumped as garbage",
                    client_version.0, client_version.1, client_version.2, client_version.3
                );
                crypt::CipherKind::None
            }
        }
    };

    // The IP the client must use to reach the proxy for the game connection.
    // Use the listen IP unless it is 0.0.0.0, in which case fall back to a
    // loopback guess (user should run with an explicit listen IP otherwise).
    let listen_ip: IpAddr = listen
        .rsplit(':')
        .next()
        .and_then(|ip| ip.parse().ok())
        .unwrap_or(IpAddr::V4(Ipv4Addr::LOCALHOST));
    let connect_ip_for_client = match listen_ip {
        IpAddr::V4(ip) if ip != Ipv4Addr::UNSPECIFIED => ip,
        _ => Ipv4Addr::LOCALHOST,
    };

    Some(Config {
        listen,
        connect,
        output,
        filter,
        json,
        timestamps,
        client_version,
        rewrite_relay,
        game_port_base,
        connect_ip_for_client,
        cipher: resolved,
        crypt_seed,
    })
}

fn main() {
    let cfg = match parse_args() {
        Some(c) => c,
        None => std::process::exit(1),
    };

    // Provisional table: the real version is deduced from the client's 0xEF
    // connection packet; pre-6.0.6.0 clients never send one and keep this.
    packets::set_table(cfg.client_version);

    let out = std::fs::File::create(&cfg.output).expect("cannot create output file");
    let json_on = cfg.json;
    let cipher = cfg.cipher;
    let shared = Arc::new(Shared {
        out: Mutex::new(out),
        decoders: if json_on {
            decode::default_registry()
        } else {
            decode::Registry::new()
        },
        proxies: Mutex::new(std::collections::HashMap::new()),
        game_seed: Mutex::new(None),
        cfg,
    });

    let listen_addr = shared.cfg.listen.clone();
    let listener = TcpListener::bind(&listen_addr).expect("cannot bind listen address");
    println!(
        "xuopacketdump: proxy {} -> {}, dump={}, stage filters: {}, crypt: {:?}",
        listen_addr,
        shared.cfg.connect,
        shared.cfg.output,
        if shared.cfg.json { "json" } else { "hex" },
        cipher
    );

    // Login stage accepts on the main listener; the stage is per-listener so
    // a reconnecting client (new login attempt after a relay) is never
    // mis-classified as a game connection by a stale stage flag.
    {
        let shared = Arc::clone(&shared);
        thread::spawn(move || {
            for stream in listener.incoming() {
                match stream {
                    Ok(s) => {
                        let shared = Arc::clone(&shared);
                        thread::spawn(move || handle_connection(shared, s, Stage::Login));
                    }
                    Err(_) => continue,
                }
            }
        });
    }

    // Keep the process alive; game-stage listeners run on their own threads,
    // spawned on demand when the shard advertises relay targets via 0x8C.
    thread::park();
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_shared(game_port_base: u16) -> Arc<Shared> {
        let cfg = Config {
            listen: "127.0.0.1:0".into(),
            connect: "127.0.0.1:1".into(),
            output: "/dev/null".into(),
            filter: Filter::default(),
            json: false,
            timestamps: false,
            client_version: (1, 25, 32, 0),
            rewrite_relay: true,
            game_port_base,
            connect_ip_for_client: Ipv4Addr::LOCALHOST,
            cipher: crypt::CipherKind::None,
            crypt_seed: 0x1357,
        };
        Arc::new(Shared {
            out: Mutex::new(std::fs::File::create("/dev/null").unwrap()),
            cfg,
            decoders: decode::Registry::new(),
            proxies: Mutex::new(std::collections::HashMap::new()),
            game_seed: Mutex::new(None),
        })
    }

    #[test]
    fn relay_rewrite_points_client_at_a_proxy_port() {
        let shared = test_shared(25950);
        let mut pkt = vec![0x8C, 127, 0, 0, 1, 0x0A, 0x23, 0xC7, 0x53, 0xCA, 0x99];
        assert!(on_packet_from_server(&shared, Stage::Login, &mut pkt));
        // loopback relay IP -> --connect host, a local proxy port >= the base
        // (allocation skips ports already in use, so only the lower bound holds)
        assert_eq!(&pkt[1..5], &[127, 0, 0, 1]);
        assert!(u16::from_be_bytes([pkt[5], pkt[6]]) >= 25950);
        // wire seed (client/game crypt key) must survive untouched
        assert_eq!(&pkt[7..11], &[0xC7, 0x53, 0xCA, 0x99]);
        assert_eq!(*shared.game_seed.lock().unwrap(), Some([0xC7, 0x53, 0xCA, 0x99]));
    }

    #[test]
    fn distinct_real_servers_get_distinct_ports() {
        let shared = test_shared(25950);
        // two distinct non-loopback game servers must not share a proxy port
        let (mut a, mut b) = (
            vec![0x8C, 10, 0, 0, 7, 0x0A, 0x23, 0, 0, 0, 0],
            vec![0x8C, 10, 0, 0, 8, 0x0A, 0x23, 0, 0, 0, 0],
        );
        on_packet_from_server(&shared, Stage::Login, &mut a);
        on_packet_from_server(&shared, Stage::Login, &mut b);
        let (pa, pb) = (
            u16::from_be_bytes([a[5], a[6]]),
            u16::from_be_bytes([b[5], b[6]]),
        );
        assert_ne!(pa, pb, "each real server needs its own proxy port");
        // replaying the same relay reuses its proxy port
        let mut a2 = vec![0x8C, 10, 0, 0, 7, 0x0A, 0x23, 0, 0, 0, 0];
        on_packet_from_server(&shared, Stage::Login, &mut a2);
        assert_eq!(u16::from_be_bytes([a2[5], a2[6]]), pa);
    }

    /// Real 0xA8 from the uo1998 shard (86 bytes). Locks the record layout:
    /// count at [4..6], records from [8], 38 bytes each.
    #[test]
    fn server_list_layout_matches_shard() {
        let hex = concat!(
            "A800565D00020000554F3A20313939380000000000000000",
            "000000000000000000000000000000000000D890E6910001",
            "546573742043656E74657200000000000000000000000000",
            "00000000000000000000D890E691",
        );
        let pkt: Vec<u8> = (0..hex.len() / 2)
            .map(|i| u8::from_str_radix(&hex[i * 2..i * 2 + 2], 16).unwrap())
            .collect();
        assert_eq!(pkt.len(), 86, "fixture must be the real 86-byte packet");
        assert_eq!(u16::from_be_bytes([pkt[1], pkt[2]]), 86, "declared length");
        assert_eq!(u16::from_be_bytes([pkt[4], pkt[5]]), 2, "server count");

        let recs = |p: &[u8]| -> Vec<(u16, String, Ipv4Addr)> {
            let mut out = Vec::new();
            let mut pos = 6;
            for _ in 0..u16::from_be_bytes([p[4], p[5]]) {
                let id = u16::from_be_bytes([p[pos], p[pos + 1]]);
                let name = String::from_utf8_lossy(
                    &p[pos + 2..pos + 34].iter().copied().take_while(|&b| b != 0).collect::<Vec<u8>>(),
                )
                .to_string();
                let ip = Ipv4Addr::new(p[pos + 36], p[pos + 37], p[pos + 38], p[pos + 39]);
                out.push((id, name, ip));
                pos += 40;
            }
            out
        };
        let got = recs(&pkt);
        assert_eq!(
            got,
            vec![
                (0u16, "UO: 1998".to_string(), Ipv4Addr::new(216, 144, 230, 145)),
                (1u16, "Test Center".to_string(), Ipv4Addr::new(216, 144, 230, 145)),
            ],
            "server list must decode to the shard's real entries"
        );
    }

    #[test]
    fn loopback_relay_reuses_login_target() {
        // uo1998 relays to its own 127.0.0.1:2599, unreachable remotely;
        // mirror IgnoreRelay and go back to the login server address+port.
        let shared = test_shared(25950);
        let target = translate_relay(&shared, Ipv4Addr::LOCALHOST, 2599);
        assert_eq!(target, shared.cfg.connect);
    }

    #[test]
    fn relay_rewrite_ignored_outside_login_stage() {
        let shared = test_shared(25950);
        let mut pkt = vec![0x8C, 10, 0, 0, 5, 0x0A, 0x23, 1, 2, 3, 4];
        let before = pkt.clone();
        assert!(!on_packet_from_server(&shared, Stage::Game, &mut pkt));
        assert_eq!(pkt, before);
    }
}
