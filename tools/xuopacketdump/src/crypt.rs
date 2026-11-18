// Login encryption mirror of crossuo:
//   - stream cipher:  src/Crypt/LoginCrypt.cpp (Encrypt / Encrypt_Old /
//     Encrypt_1_25_36)
//   - key derivation: src/Config.cpp SetClientCrypt (POL cryptkey.cpp)
//   - stream init:    src/Crypt/LoginCrypt.cpp CLoginCrypt::Init
//
// Pre-6.0.6.0 clients (like the uo1998 1.25.32 client) encrypt every post-seed
// C->S byte on both the login and the game socket (the game socket re-seeds the
// same stream with the relay seed carried by 0x8C). S->C stays plaintext.
// The proxy decrypts a local copy for framing/dumping only - the original
// encrypted bytes are forwarded untouched.

use super::packets;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum CipherKind {
    None,
    Old,    // ET_OLD_BFISH: version < 1.25.35 (e.g. 1.25.32 / uo1998)
    V12536, // ET_1_25_36: exactly 1.25.36
    BFish,  // ET_BFISH: 1.25.37..2.0.0 (the "Encrypt" stream form)
}

const VER_1_25_35: u32 = 0x0119_2300;
const VER_1_25_36: u32 = 0x0119_2310;
const VER_2_0_0: u32 = 0x0200_0000;

/// Key derivation mirrored from Config.cpp SetClientCrypt (POL cryptkey.cpp).
/// Returns (k1, k2, k3) as [k1, k2, k3].
pub fn derive_keys_for(version: packets::ClientVersion) -> [u32; 3] {
    let version = ((version.0 as u32) << 24)
        | ((version.1 as u32) << 16)
        | ((version.2 as u32) << 8)
        | version.3 as u32;
    let (k1, k2, k3) = derive_keys(version);
    [k1, k2, k3]
}

fn derive_keys(version: u32) -> (u32, u32, u32) {
    let a = ((version >> 24) & 0xff) as u32;
    let b = ((version >> 16) & 0xff) as u32;
    let c = ((version >> 8) & 0xff) as u32;

    let mut temp = ((a << 9 | b) << 10 | c) ^ ((c.wrapping_mul(c)) << 5);
    let k2 = (temp << 4)
        ^ b.wrapping_mul(b)
        ^ b.wrapping_mul(0x0B00_0000)
        ^ c.wrapping_mul(0x0038_0000)
        ^ 0x2C13_A5FD;
    temp = (((a << 9 | c) << 10 | b).wrapping_mul(8)) ^ (c.wrapping_mul(c).wrapping_mul(0x0c00));
    let k3 = temp
        ^ b.wrapping_mul(b)
        ^ b.wrapping_mul(0x0680_0000)
        ^ c.wrapping_mul(0x001c_0000)
        ^ 0xA31D_527F;
    let k1 = k2.wrapping_sub(1);
    (k1, k2, k3)
}

/// Mirror of SetClientCrypt's encryption-type selection for the old, purely
/// stream-based login ciphers. Returns None for 2.0.3+ (blowfish/twofish,
/// not supported by this proxy).
pub fn cipher_for_version(version: u32) -> Option<CipherKind> {
    if version < VER_1_25_35 {
        Some(CipherKind::Old)
    } else if version == VER_1_25_36 {
        Some(CipherKind::V12536)
    } else if version < VER_2_0_0 {
        Some(CipherKind::BFish)
    } else {
        None
    }
}

pub struct LoginCrypt {
    key: [u32; 2],
    k: [u32; 3],
}

impl LoginCrypt {
    /// seed_key mirrors g_Config.Seed (default 0x1357 when no UseKeys config).
    pub fn new(seed: &[u8; 4], seed_key: u32, k: [u32; 3]) -> Self {
        let seed = u32::from_be_bytes(*seed);
        let key0 = ((!seed ^ seed_key) << 16) | ((seed ^ 0xffff_aaaa) & 0x0000_ffff);
        let key1 = ((seed ^ 0x4321_0000) >> 16) | ((!seed ^ 0xabcd_ffff) & 0xffff_0000);
        LoginCrypt {
            key: [key0, key1],
            k,
        }
    }

    pub fn from_version(seed: &[u8; 4], version: u32, seed_key: u32) -> Option<Self> {
        cipher_for_version(version)
            .map(|_| LoginCrypt::new(seed, seed_key, derive_keys(version).into()))
    }

    fn step_old(&mut self, t0: u32, t1: u32) {
        self.key[0] = ((t0 >> 1) | (t1 << 31)) ^ self.k[2];
        self.key[1] = ((t1 >> 1) | (t0 << 31)) ^ self.k[1];
    }

    fn step_1_25_36(&mut self, t0: u32, t1: u32) {
        self.key[0] = ((t0 >> 1) | (t1 << 31)) ^ self.k[1];
        self.key[1] = ((t1 >> 1) | (t0 << 31)) ^ self.k[0];
        self.key[1] = (self.k[0] >> ((5u32.wrapping_mul(t1).wrapping_mul(t1)) & 0x1F))
            .wrapping_add(t1.wrapping_mul(self.k[0]))
            .wrapping_add(t0.wrapping_mul(t0).wrapping_mul(0x35ce_9581))
            .wrapping_add(0x07af_cc37);
        self.key[0] = (self.k[1] >> ((3u32.wrapping_mul(t0).wrapping_mul(t0)) & 0x1F))
            .wrapping_add(t0.wrapping_mul(self.k[1]))
            .wrapping_add(
                self.key[1]
                    .wrapping_mul(self.key[1])
                    .wrapping_mul(0x4c3a_1353),
            )
            .wrapping_add(0x16ef_783f);
    }

    fn step_bfish(&mut self, t0: u32, t1: u32) {
        self.key[1] = ((((t1 >> 1) | (t0 << 31)) ^ self.k[0]) >> 1 | (t0 << 31)) ^ self.k[1];
        self.key[0] = ((t0 >> 1) | (t1 << 31)) ^ self.k[2];
    }

    /// Decrypt/encrypt in place (the stream cipher is symmetric XOR).
    /// Kind is fixed at construction; this matches the variant chosen.
    pub fn xform(&mut self, data: &mut [u8], kind: CipherKind) {
        for byte in data.iter_mut() {
            *byte ^= self.key[0] as u8;
            let (t0, t1) = (self.key[0], self.key[1]);
            match kind {
                CipherKind::Old => self.step_old(t0, t1),
                CipherKind::V12536 => self.step_1_25_36(t0, t1),
                CipherKind::BFish => self.step_bfish(t0, t1),
                CipherKind::None => {}
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn keys(version: u32) -> [u32; 3] {
        derive_keys(version).into()
    }

    /// Cross-check against the wire capture from the uo1998 shard: crossuo's
    /// own C++ Encrypt_Old produced 55 CA 65 A0 46 B9 DE ... for the plaintext
    /// 0x80 First Login packet ("user"/"pass"), seed 01 01 00 7F.
    #[test]
    fn old_wire_capture_roundtrip() {
        let seed = [0x01u8, 0x01, 0x00, 0x7f];
        let version = 0x0119_2000; // 1.25.32.0
        let k = keys(version);
        let mut pt = [0u8; 62];
        pt[0] = 0x80;
        pt[1..5].copy_from_slice(b"user");
        pt[31..35].copy_from_slice(b"pass");
        pt[61] = 0xFF;

        let mut c = LoginCrypt::new(&seed, 0x1357, k);
        let mut buf = pt;
        c.xform(&mut buf, CipherKind::Old);
        assert_eq!(
            &buf[..7],
            &[0x55, 0xD9, 0x63, 0xAB, 0x53, 0xD6, 0xAD],
            "cipher must match the C++ oracle for user/pass"
        );
        // decrypt back
        let mut d = LoginCrypt::new(&seed, 0x1357, k);
        let mut round = buf;
        d.xform(&mut round, CipherKind::Old);
        assert_eq!(round, pt);
    }

    #[test]
    fn v12536_roundtrip() {
        let seed = [0x7fu8, 0x00, 0x00, 0x01];
        let version = VER_1_25_36;
        let k = keys(version);
        let pt: Vec<u8> = (0..62u8).collect();
        let mut c = LoginCrypt::new(&seed, 0x1357, k);
        let mut buf = pt.clone();
        c.xform(&mut buf, CipherKind::V12536);
        let mut d = LoginCrypt::new(&seed, 0x1357, k);
        d.xform(&mut buf, CipherKind::V12536);
        assert_eq!(buf, pt);
    }

    #[test]
    fn bfish_roundtrip() {
        let seed = [0xc0u8, 0xa8, 0x02, 0xcd];
        let version = 0x0119_2340; // 1.25.37.0
        let k = keys(version);
        let pt: Vec<u8> = (0..62u8).collect();
        let mut c = LoginCrypt::new(&seed, 0x1357, k);
        let mut buf = pt.clone();
        c.xform(&mut buf, CipherKind::BFish);
        let mut d = LoginCrypt::new(&seed, 0x1357, k);
        d.xform(&mut buf, CipherKind::BFish);
        assert_eq!(buf, pt);
    }
}
