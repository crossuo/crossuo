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
//      address. If --rewrite-relay is on (default), the relay IP/port inside
//      the passing 0x8C packet is rewritten to point back at this proxy, so
//      the game connection is captured too; it is flagged STAGE GAME.
mod decode;
mod filter;
mod format;
mod framer;
mod packets;
mod special;

use std::io::{Read, Write};
use std::net::{IpAddr, Ipv4Addr, TcpListener, TcpStream};
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
    game_listen: String,
    connect_ip_for_client: Ipv4Addr,
}

struct Shared {
    out: Mutex<std::fs::File>,
    stage: Mutex<Stage>,
    cfg: Config,
    decoders: decode::Registry,
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

/// Handle connection-level interception logic. Returns true when the proxy
/// altered the packet bytes (relay rewrite).
fn on_packet_from_server(shared: &Shared, pkt: &mut Vec<u8>) -> bool {
    if pkt[0] == 0x8C && pkt.len() >= 11 && shared.cfg.rewrite_relay {
        // 0x8C Relay Server: [0]=0x8C [1..5]=ip [5..7]=port [7..11]=seed
        let game_port: u16 = shared
            .cfg
            .game_listen
            .rsplit(':')
            .next()
            .and_then(|p| p.parse().ok())
            .unwrap_or(2595);
        let ip = shared.cfg.connect_ip_for_client.octets();
        pkt[1..5].copy_from_slice(&ip);
        pkt[5..7].copy_from_slice(&game_port.to_be_bytes());
        let mut out = shared.out.lock().unwrap();
        let _ = writeln!(
            &mut *out,
            "REWRITE 0x8C relay -> {}:{}",
            shared.cfg.connect_ip_for_client, game_port
        );
        let _ = out.flush();
        return true;
    }
    false
}

fn pump(
    shared: Arc<Shared>,
    mut reader: TcpStream,
    mut writer: TcpStream,
    to_server: bool,
    expect_seed: bool,
) {
    let stage = *shared.stage.lock().unwrap();
    let mut framer = Framer::new();
    let mut seeded = !expect_seed;
    let mut buf = [0u8; 0x1000];

    loop {
        let n = match reader.read(&mut buf) {
            Ok(0) => {
                // flush complete packets still pending in the framer
                while let Some(mut pkt) = framer.next_packet() {
                    if to_server && pkt[0] == 0xEF {
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
                    }
                    if !to_server {
                        on_packet_from_server(&shared, &mut pkt);
                    }
                    write_packet(&shared, to_server, &pkt);
                }
                break;
            }
            Ok(n) => n,
            Err(_) => break,
        };
        let chunk = &buf[..n];

        if !seeded {
            // The first bytes of a client connection are either a bare 4-byte
            // seed (< 6.0.6.0 and every game connection) or a 0xEF packet
            // (seed + client version) - see CConnectionManager::Connect.
            if to_server && framer.pending().is_empty() {
                // buffer until we have 4 bytes to identify the form
                framer.push(chunk);
                if framer.pending().len() < 4 {
                    // partial seed - forward and wait for the rest
                    if writer.write_all(chunk).is_err() {
                        break;
                    }
                    continue;
                }
                // 0xEF is only ever sent on the login connection
                // (ConnectionManager.cpp:157-181); game connections always
                // use a bare seed (ConnectionManager.cpp:202).
                if stage == Stage::Login {
                    if framer.pending()[0] == 0xEF {
                        // 0xEF is a normal framed packet (0x15 bytes) carrying
                        // the version; leave it for the framer and the deduce step.
                        seeded = true;
                    } else {
                        let seed: Vec<u8> = framer.pending()[..4].to_vec();
                        // keep the remainder as framed data: re-create framer state
                        let rest: Vec<u8> = framer.pending()[4..].to_vec();
                        framer.clear();
                        framer.push(&rest);
                        write_seed(&shared, stage, &seed);
                        seeded = true;
                    }
                } else {
                    // game stage: always a bare 4-byte seed
                    let seed: Vec<u8> = framer.pending()[..4].to_vec();
                    let rest: Vec<u8> = framer.pending()[4..].to_vec();
                    framer.clear();
                    framer.push(&rest);
                    write_seed(&shared, stage, &seed);
                    seeded = true;
                }
            } else {
                framer.push(chunk);
                seeded = true;
            }
        } else {
            framer.push(chunk);
        }

        while let Some(mut pkt) = framer.next_packet() {
            if to_server {
                // The client announces its version in the 0xEF connection
                // packet (seed + major/minor/rev/prot BE u32s) - mirrors
                // CConnectionManager::Connect (ConnectionManager.cpp:157-181).
                // Clients < 6.0.6.0 send a bare seed instead, so the fallback
                // table from --client-version stays in effect for them.
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
            }
            if !to_server {
                on_packet_from_server(&shared, &mut pkt);
                if pkt[0] == 0x8C {
                    *shared.stage.lock().unwrap() = Stage::Game;
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

        if writer.write_all(chunk).is_err() {
            break;
        }
    }
    // half-close only: EOF on this leg means the upstream peer ended its
    // direction; propagate EOF downstream without killing the other leg
    // (a server closing after a response must not cut the client's replies)
    let _ = writer.shutdown(std::net::Shutdown::Write);
}

fn handle_connection(shared: Arc<Shared>, client: TcpStream, stage: Stage) {
    let peer = client.peer_addr().map(|a| a.to_string()).unwrap_or_default();
    write_marker(&shared, &format!("CONNECT STAGE={} from {}", stage.name(), peer));

    let server = match TcpStream::connect(&shared.cfg.connect) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("xuopacketdump: cannot connect to {}: {}", shared.cfg.connect, e);
            write_marker(&shared, &format!("CONNECT FAILED to {}: {}", shared.cfg.connect, e));
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

    let sh1 = Arc::clone(&shared);
    let sh2 = Arc::clone(&shared);
    let t1 = thread::spawn(move || pump(sh1, c2s_client, c2s_server, true, true));
    let t2 = thread::spawn(move || pump(sh2, s2c_server, s2c_client, false, false));
    let _ = t1.join();
    let _ = t2.join();

    write_marker(&shared, &format!("DISCONNECT STAGE={}", stage.name()));
}

const USAGE: &str = "\
xuopacketdump - UO client/server packet dump proxy

USAGE:
  xuopacketdump --listen <ip:port> --connect <host:port> [options]

OPTIONS:
  -l, --listen <ip:port>     local proxy address the client connects to
  -c, --connect <host:port>  real server address to forward to
  -o, --output <file>        dump file (default: xuopacketdump.txt)
  -f, --filter <file>        filter config: packet IDs to ignore (denoise)
  -j, --json                 decode registered packets as JSON lines
  -t, --timestamps           add a TS epoch line per packet
  -v, --client-version <ver> fallback client version for pre-6.0.6.0 clients (which send no
                             0xEF packet); otherwise deduced from the 0xEF connection packet
      --game-port <port>     local port for the relayed game connection (default 2595)
      --no-rewrite-relay     do not rewrite 0x8C to route the game connection
                             back through the proxy
  -h, --help
";

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
    let mut game_listen_port = 2595u16;

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
                    game_listen_port = v.parse().unwrap_or(2595);
                }
            }
            "--no-rewrite-relay" => rewrite_relay = false,
            other => {
                eprintln!("xuopacketdump: unknown option '{}'", other);
                return None;
            }
        }
        i += 1;
    }

    let listen = listen?;
    let connect = connect?;

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
        game_listen: format!("0.0.0.0:{}", game_listen_port),
        connect_ip_for_client,
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
    let shared = Arc::new(Shared {
        out: Mutex::new(out),
        stage: Mutex::new(Stage::Login),
        decoders: if json_on {
            decode::default_registry()
        } else {
            decode::Registry::new()
        },
        cfg,
    });

    let listen_addr = shared.cfg.listen.clone();
    let listener = TcpListener::bind(&listen_addr).expect("cannot bind listen address");
    println!(
        "xuopacketdump: proxy {} -> {}, dump={}, stage filters: {}",
        listen_addr,
        shared.cfg.connect,
        shared.cfg.output,
        if shared.cfg.json { "json" } else { "hex" }
    );

    // Game-stage listener is opened lazily when the relay packet arrives,
    // but we bind it up front so the rewritten 0x8C port is reachable.
    let game_addr = shared.cfg.game_listen.clone();
    let game_listener = TcpListener::bind(&game_addr).expect("cannot bind game listen port");

    // Login stage accepts on the main listener.
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

    // Game stage accepts on the secondary listener; the stage flag is already
    // GAME by the time the client dials the rewritten relay address.
    for stream in game_listener.incoming() {
        match stream {
            Ok(s) => {
                let shared = Arc::clone(&shared);
                let stage = *shared.stage.lock().unwrap();
                thread::spawn(move || handle_connection(shared, s, stage));
            }
            Err(_) => continue,
        }
    }
}

