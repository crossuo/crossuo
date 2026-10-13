// Diff-friendly dump formatting.
//
// Rules:
//  - one blank line between blocks, nothing else freeform
//  - fixed-width columns (offsets and hex bytes are zero padded), so the
//    ASCII column never shifts
//  - packet size on its own header line: size-only changes are one-line diffs
//  - no timestamps by default (--timestamps adds one as a separate header line)
use std::io::Write;

pub fn header(out: &mut impl Write, dir: &str, id: u8, name: &str, size: usize) -> std::io::Result<()> {
    writeln!(out)?;
    writeln!(out, "{}", dir)?;
    writeln!(out, "0x{:02X} {} ({})", id, name, size)
}

pub fn marker(out: &mut impl Write, text: &str) -> std::io::Result<()> {
    writeln!(out)?;
    writeln!(out, "== {} ==", text)
}

pub fn hexdump(out: &mut impl Write, data: &[u8]) -> std::io::Result<()> {
    let base = 0usize;
    for (row, chunk) in data.chunks(16).enumerate() {
        write!(out, "{:04X}: ", base + row * 16)?;
        let mut ascii = String::with_capacity(16);
        for i in 0..16 {
            match chunk.get(i) {
                Some(&b) => {
                    write!(out, "{:02X} ", b)?;
                    ascii.push(if (0x20..0x7f).contains(&b) { b as char } else { '.' });
                }
                None => {
                    write!(out, "   ")?;
                    ascii.push(' ');
                }
            }
            if i == 7 {
                write!(out, " ")?;
            }
        }
        writeln!(out, " {}", ascii)?;
    }
    Ok(())
}
