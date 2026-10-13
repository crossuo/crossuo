// Incremental packet framing, mirroring Wisp::CPacketMessage::Read
// (src/Wisp/WispPacketMessage.cpp) combined with
// CPacketManager::GetPacketSize (PacketManager.cpp:644):
//   fixed size     -> table size
//   variable (0)   -> big-endian u16 at offset 1 (needs >= 3 bytes buffered)
pub struct Framer {
    buf: Vec<u8>,
}

impl Framer {
    pub fn new() -> Self {
        Framer { buf: Vec::new() }
    }

    pub fn push(&mut self, data: &[u8]) {
        self.buf.extend_from_slice(data);
    }

    /// Try to extract one complete packet. Returns None if more data is needed.
    pub fn next_packet(&mut self) -> Option<Vec<u8>> {
        let guard = crate::packets::table();
        let table = guard.as_ref().expect("packet table not initialized");
        loop {
            if self.buf.is_empty() {
                return None;
            }
            let id = self.buf[0];
            let mut want = table[id as usize].size as usize;
            if want == 0 {
                if self.buf.len() < 3 {
                    return None;
                }
                want = ((self.buf[1] as usize) << 8) | self.buf[2] as usize;
                if want < 3 {
                    // malformed: drop the id byte to make progress
                    self.buf.remove(0);
                    continue;
                }
            }
            if self.buf.len() < want {
                return None;
            }
            let packet = self.buf[..want].to_vec();
            self.buf.drain(..want);
            return Some(packet);
        }
    }

    /// Bytes remaining in the buffer (used to flush the leading seed).
    pub fn pending(&self) -> &[u8] {
        &self.buf
    }

    pub fn clear(&mut self) {
        self.buf.clear();
    }
}
