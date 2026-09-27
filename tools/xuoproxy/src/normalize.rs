//! Normalization of streaming transfers for the web client.
//!
//! The upstream patch storage keeps some content in shapes the web client
//! cannot consume punctually:
//!
//! * transfers compressed as a whole (`ct = 1`, zlib): served inflated, so
//!   the manifest rewrite (`ct -> 0`, `cl := ul`) stays truthful
//! * `.uop` containers whose blocks use encodings the client's refine walk
//!   refuses (Flags == 3, the Burrows-Wheeler layout gumpart ships with):
//!   rebuilt with every block stored uncompressed (Flags = 0), so the walk
//!   splits the pack into per-block range assets like any other
//!
//! Everything else passes through byte-identical.

use std::io::{Read, Write};
use std::collections::HashMap;
use std::ops::Range;
use std::sync::OnceLock;

// ---------------------------------------------------------------------------
// zlib
// ---------------------------------------------------------------------------

pub fn zlib_inflate(data: &[u8]) -> Option<Vec<u8>>
{
    let mut out = Vec::new();
    let mut decoder = flate2::read::ZlibDecoder::new(data);
    decoder.read_to_end(&mut out).ok()?;
    Some(out)
}

pub fn zlib_deflate(data: &[u8]) -> Vec<u8>
{
    use std::io::Write;
    let mut encoder = flate2::write::ZlibEncoder::new(Vec::new(), flate2::Compression::default());
    encoder
        .write_all(data)
        .and_then(|_| encoder.finish())
        .unwrap_or_else(|_| data.to_vec())
}

pub fn looks_zlib(data: &[u8]) -> bool
{
    // 0x78 followed by a check byte making the first two bytes a valid
    // big-endian multiple of 31 (0x08..0x7f window sizes)
    data.len() >= 2
        && data[0] == 0x78
        && (((data[0] as u16) << 8) | data[1] as u16) % 31 == 0
}

// ---------------------------------------------------------------------------
// Burrows-Wheeler decoder for MYP entries with Flags == 3
// (port of xuocore/uodata.cpp bwt_decompress; the layout matches
// ClassicUO's reader: zlib stream -> 16-bit MTF pass -> 1KB frequency
// header -> two coded streams)
// ---------------------------------------------------------------------------

pub fn bwt_decompress(coded: &[u8]) -> Option<Vec<u8>>
{
    if coded.len() < 6
    {
        return None;
    }

    // 16-bit move-to-front table; the reference builds the ascending 16-bit
    // sequence and sorts it, which is exactly the identity
    let mut table: Vec<u16> = (0..=u16::MAX).collect();

    let list_len = coded.len() - 4;
    let mut list = vec![0u8; list_len];
    let mut first = coded[4];
    let mut li = 0usize;
    for pos in 5..coded.len()
    {
        if li >= list_len
        {
            break;
        }
        let current_value = first;
        let value = table[current_value as usize];
        if current_value > 0
        {
            for cv in (1..=current_value as usize).rev()
            {
                table[cv] = table[cv - 1];
            }
        }
        table[0] = value;
        list[li] = value as u8;
        first = coded[pos];
        li += 1;
    }

    // the coded stream carries a 1KB frequency header followed by the two
    // coded streams, laid out exactly as ClassicUO reads it
    if list.len() < 2048
    {
        return None;
    }

    let mut partial_input = [0i32; 256 * 3];
    for (i, byte) in list[..1024].iter().enumerate()
    {
        // little-endian int pairs, exactly as the C code memcpy's them in
        partial_input[i / 4] |= i32::from(*byte) << (8 * (i % 4));
    }

    let sum: i32 = partial_input[..256].iter().sum();
    if sum <= 0
    {
        return None;
    }

    let mut out = vec![0u8; sum as usize];

    let mut non_zero_count: i32 = 0;
    for fi in 0..256
    {
        if partial_input[fi] != 0
        {
            non_zero_count += 1;
        }
    }

    let mut frequency = [0u8; 256];
    {
        let mut tmp = [0i32; 256];
        tmp.copy_from_slice(&partial_input[..256]);
        for fi in 0..256
        {
            let mut best_value = 0u32;
            let mut best_index = 0u8;
            for (fj, &v) in tmp.iter().enumerate()
            {
                if v as u32 > best_value
                {
                    best_index = fj as u8;
                    best_value = v as u32;
                }
            }
            if best_value == 0
            {
                break;
            }
            frequency[fi] = best_index;
            tmp[best_index as usize] = 0;
        }
    }

    let mut symbol_table = [0u8; 256];
    for (i, s) in symbol_table.iter_mut().enumerate()
    {
        *s = i as u8;
    }

    let mut m: i32 = 0;
    for i in 0..non_zero_count as usize
    {
        let freq = frequency[i];
        symbol_table[list[m as usize + 1024] as usize] = freq;
        partial_input[(freq as usize) + 256] = m + 1;
        m += partial_input[freq as usize];
        partial_input[(freq as usize) + 512] = m;
    }

    let mut val = symbol_table[0];
    let mut count = 0usize;
    while count < sum as usize
    {
        let first_val = partial_input[(val as usize) + 256];
        out[count] = val;
        if first_val >= partial_input[(val as usize) + 512]
        {
            if non_zero_count > 0
            {
                non_zero_count -= 1;
                // shift_left(nonZeroCount) - the post-decremented value
                for i in 0..non_zero_count as usize
                {
                    symbol_table[i] = symbol_table[i + 1];
                }
                val = symbol_table[0];
            }
        }
        else
        {
            let idx = list[(first_val as usize) + 1024];
            partial_input[(val as usize) + 256] += 1;
            if idx != 0
            {
                for i in 0..idx as usize
                {
                    symbol_table[i] = symbol_table[i + 1];
                }
                symbol_table[idx as usize] = val;
                val = symbol_table[0];
            }
        }
        count += 1;
    }
    Some(out)
}

// ---------------------------------------------------------------------------
// uop container parse + normalization
// ---------------------------------------------------------------------------

pub const MYP_MAGIC: u32 = 0x0050_594D; // 'MYP\0'

struct UopEntry
{
    metadata_size: u32,
    decompressed_size: u32,
    hash: u64,
    checksum: u32,
    flags: u16,
    // block bytes as stored (metadata excluded)
    payload: Vec<u8>,
}

fn rd_u16(d: &[u8], at: usize) -> Option<u16>
{
    d.get(at..at + 2).map(|b| u16::from_le_bytes([b[0], b[1]]))
}
fn rd_u32(d: &[u8], at: usize) -> Option<u32>
{
    d.get(at..at + 4).map(|b| u32::from_le_bytes([b[0], b[1], b[2], b[3]]))
}
fn rd_u64(d: &[u8], at: usize) -> Option<u64>
{
    d.get(at..at + 8).map(|b| {
        u64::from_le_bytes([b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]])
    })
}

/// Parses a Mythic Package container and returns its blocks
/// (metadata + payload as stored). None when the data is not a uop
/// container (v4/v5) or is structurally broken.
fn uop_parse(data: &[u8]) -> Option<Vec<UopEntry>>
{
    if data.len() < 40
    {
        return None;
    }
    if rd_u32(data, 0)? != MYP_MAGIC
    {
        return None;
    }
    let version = rd_u32(data, 4)?;
    if !(4..=5).contains(&version)
    {
        return None;
    }
    let mut next = rd_u64(data, 12)? as usize;
    let mut entries = Vec::new();
    let mut guard = 0usize;
    while next != 0
    {
        guard += 1;
        if guard > 100_000 || next + 12 > data.len()
        {
            return None;
        }
        let file_count = rd_u32(data, next)?;
        let following = rd_u64(data, next + 4)?;
        let table = next + 12;
        if file_count == 0
        {
            return None; // a chain never carries empty sections
        }
        let table_bytes = (file_count as usize) * 34;
        if table + table_bytes > data.len()
        {
            return None;
        }
        for i in 0..file_count as usize
        {
            let at = table + i * 34;
            let offset = rd_u64(data, at)?;
            let metadata_size = rd_u32(data, at + 8)?;
            let compressed_size = rd_u32(data, at + 12)?;
            let decompressed_size = rd_u32(data, at + 16)?;
            let hash = rd_u64(data, at + 20)?;
            let checksum = rd_u32(data, at + 28)?;
            let flags = rd_u16(data, at + 32)?;
            if offset == 0
            {
                continue; // hole: reserved slot, no bytes anywhere
            }
            let end = (offset as usize)
                .checked_add(metadata_size as usize)?
                .checked_add(compressed_size as usize)?;
            if end > data.len()
            {
                return None; // entry points outside the file: not a table we trust
            }
            let block = data[offset as usize..end].to_vec();
            entries.push(UopEntry {
                metadata_size,
                decompressed_size,
                hash,
                checksum,
                flags,
                payload: block,
            });
        }
        next = following as usize;
    }
    if entries.is_empty()
    {
        return None;
    }
    Some(entries)
}

/// True when the container's blocks use encodings the client's refine walk
/// refuses (everything but stored/zlib - in practice the BWT blocks of
/// gumpartLegacyMUL.uop). Such containers are rebuilt with every block
/// stored, which turns them into ordinary range-able sources.
pub fn uop_needs_normalization(data: &[u8]) -> bool
{
    match uop_parse(data)
    {
        Some(entries) => entries.iter().any(|e| e.flags != 0 && e.flags != 1),
        None => false,
    }
}

/// Rebuilds the container with every block stored uncompressed
/// (Flags = 0, CompressedSize = DecompressedSize), preserving the header
/// fields, the entry order, hashes, checksums and each block's metadata.
pub fn uop_normalize(data: &[u8]) -> Option<Vec<u8>>
{
    let entries = uop_parse(data)?;

    // decode every block first (fail fast before writing anything)
    let mut blocks: Vec<(Vec<u8>, Vec<u8>)> = Vec::with_capacity(entries.len()); // (metadata, stored payload)
    for e in &entries
    {
        if e.payload.len() < e.metadata_size as usize
        {
            return None;
        }
        let meta = &e.payload[..e.metadata_size as usize];
        let stored = &e.payload[e.metadata_size as usize..];
        let plain = match e.flags
        {
            0 => stored.to_vec(),
            1 => zlib_inflate(stored)?,
            3 =>
            {
                let coded = zlib_inflate(stored)?;
                bwt_decompress(&coded)?
            }
            _ => return None,
        };
        blocks.push((meta.to_vec(), plain));
    }

    // re-layout: table at 40, blocks packed after it in entry order
    let table_size = 40 + 12 + entries.len() * 34;
    let mut offsets = Vec::with_capacity(entries.len());
    let mut cursor = table_size as u64;
    for (meta, plain) in &blocks
    {
        offsets.push(cursor);
        cursor += meta.len() as u64 + plain.len() as u64;
    }

    let mut out = Vec::with_capacity(cursor as usize);
    // header: magic, version 4, signature, section offset 40, capacity,
    // count, section count 1, unk1 1, unk2 0 (same shape the client writes)
    out.extend_from_slice(&MYP_MAGIC.to_le_bytes());
    out.extend_from_slice(&4u32.to_le_bytes());
    out.extend_from_slice(&rd_u32(data, 8)?.to_le_bytes()); // signature
    out.extend_from_slice(&40u64.to_le_bytes());
    out.extend_from_slice(&(entries.len() as u32).to_le_bytes());
    out.extend_from_slice(&(entries.len() as u32).to_le_bytes());
    out.extend_from_slice(&1u32.to_le_bytes());
    out.extend_from_slice(&1u32.to_le_bytes());
    out.extend_from_slice(&0u32.to_le_bytes());
    // section
    out.extend_from_slice(&(entries.len() as u32).to_le_bytes());
    out.extend_from_slice(&0u64.to_le_bytes()); // next section: none
    // entries
    for (e, (offset, (meta, plain))) in entries.iter().zip(offsets.iter().zip(blocks.iter()))
    {
        out.extend_from_slice(&offset.to_le_bytes());
        out.extend_from_slice(&(meta.len() as u32).to_le_bytes());
        out.extend_from_slice(&(plain.len() as u32).to_le_bytes());
        out.extend_from_slice(&(plain.len() as u32).to_le_bytes());
        out.extend_from_slice(&e.hash.to_le_bytes());
        out.extend_from_slice(&e.checksum.to_le_bytes());
        out.extend_from_slice(&0u16.to_le_bytes()); // stored
    }
    // blocks
    for (meta, plain) in &blocks
    {
        out.extend_from_slice(meta);
        out.extend_from_slice(plain);
    }
    Some(out)
}

#[cfg(test)]
mod tests
{
    use super::*;

    #[test]
    fn zlib_roundtrip()
    {
        let mut enc = flate2::write::ZlibEncoder::new(Vec::new(), flate2::Compression::default());
        enc.write_all(b"hello normalization").unwrap();
        let compressed = enc.finish().unwrap();
        assert!(looks_zlib(&compressed));
        assert_eq!(zlib_inflate(&compressed).unwrap(), b"hello normalization");
    }

    #[test]
    fn manifest_rewrite_matches_tails()
    {
        let text = "<MythicMFT><manifest><files><f n=\"gumpartLegacyMUL.uop\" ul=\"4db77c3\" ct=\"0\" cl=\"4db77c3\" t=\"1\" rh=\"2\" /></files></manifest></MythicMFT>";
        let (ph, sh) = uop_name_hashes("gumpartLegacyMUL.uop");
        let tail = format!("base/unpacked/{:08x}{:08x}", ph, sh);
        let mut sizes = std::collections::HashMap::new();
        sizes.insert(tail.clone(), 176977098u64);
        let out = rewrite_manifest_uop_entries(text, &sizes);
        let text = String::from_utf8(out).unwrap();
        // the client parses ul/cl with Hex64Attribute: sizes must be hex
        let want = format!("{:x}", 176977098u64);
        assert!(text.contains(&format!("ul=\"{want}\"")), "size not rewritten: {text}");
        assert!(text.contains("ct=\"0\""));
        assert!(!text.contains("4db77c3"), "old size still present: {text}");
        // the tag must stay well formed: exactly one of each attribute, no
        // doubled attributes from a value-span replacement
        assert_eq!(text.matches("ul=").count(), 1, "mangled tag: {text}");
        assert_eq!(text.matches("cl=").count(), 1, "mangled tag: {text}");
        assert_eq!(text.matches("ct=").count(), 1, "mangled tag: {text}");
        assert!(!text.contains("\"\""), "mangled tag: {text}");
        // and the tail lookup helper agrees
        assert!(manifest_loose_uop_tails(text.as_str(), "base").contains(&tail));
    }

    #[test]
    fn manifest_rewrite_inserts_missing_attrs()
    {
        // gumpart ships with no ct/cl at all (plain transfer)
        let text = "<files><f n=\"gumpartLegacyMUL.uop\" ul=\"4db77c3\" t=\"1\" rh=\"2\" /></files>";
        let (ph, sh) = uop_name_hashes("gumpartLegacyMUL.uop");
        let tail = format!("base/unpacked/{:08x}{:08x}", ph, sh);
        let mut sizes = std::collections::HashMap::new();
        sizes.insert(tail.clone(), 176977098u64);
        let out = String::from_utf8(rewrite_manifest_uop_entries(text, &sizes)).unwrap();
        let want = format!("{:x}", 176977098u64);
        assert!(out.contains(&format!("ct=\"0\"")), "ct not inserted: {out}");
        assert!(out.contains(&format!("cl=\"{want}\"")), "cl not inserted: {out}");
        assert!(out.contains(&format!("ul=\"{want}\"")), "ul not rewritten: {out}");
        assert!(out.contains("n=\"gumpartLegacyMUL.uop\""), "n attribute mangled: {out}");
        // well formed: exactly one of each attribute, no doubled quotes
        assert_eq!(out.matches("ul=").count(), 1, "mangled tag: {out}");
        assert_eq!(out.matches("cl=").count(), 1, "mangled tag: {out}");
        assert_eq!(out.matches("ct=").count(), 1, "mangled tag: {out}");
        assert!(!out.contains("\"\""), "mangled tag: {out}");
    }

    #[test]
    fn rejects_non_uop()
    {
        assert!(!uop_needs_normalization(b"not a uop container at all"));
        assert!(!uop_needs_normalization(&[]));
    }
}

// ---------------------------------------------------------------------------
// Jenkins lookup3 hashlittle2 (port of external/lookup3.h, byte-wise path -
// identical results to the aligned little-endian path)
// ---------------------------------------------------------------------------

fn mix(a: &mut u32, b: &mut u32, c: &mut u32)
{
    *a = a.wrapping_sub(*c);
    *a ^= c.rotate_left(4);
    *c = c.wrapping_add(*b);
    *b = b.wrapping_sub(*a);
    *b ^= a.rotate_left(6);
    *a = a.wrapping_add(*c);
    *c = c.wrapping_sub(*b);
    *c ^= b.rotate_left(8);
    *b = b.wrapping_add(*a);
    *a = a.wrapping_sub(*c);
    *a ^= c.rotate_left(16);
    *c = c.wrapping_add(*b);
    *b = b.wrapping_sub(*a);
    *b ^= a.rotate_left(19);
    *a = a.wrapping_add(*c);
    *c = c.wrapping_sub(*b);
    *c ^= b.rotate_left(4);
    *b = b.wrapping_add(*a);
}

/// lookup3 hashlittle2: returns (pc, pb) - the UOP part blob name is
/// "%08x%08x" of (pc, sh=pb) with both seeds zero.
pub fn hashlittle2(key: &[u8], mut pc: u32, mut pb: u32) -> (u32, u32)
{
    let mut a: u32;
    let mut b: u32;
    let mut c: u32;
    let mut length = key.len();
    a = 0xdeadbeefu32.wrapping_add(length as u32).wrapping_add(pc);
    c = a.wrapping_add(pb);
    b = a;

    let mut k = 0usize;
    while length > 12
    {
        a = a.wrapping_add(u32::from_le_bytes([
            key[k],
            key[k + 1],
            key[k + 2],
            key[k + 3],
        ]));
        b = b.wrapping_add(u32::from_le_bytes([
            key[k + 4],
            key[k + 5],
            key[k + 6],
            key[k + 7],
        ]));
        c = c.wrapping_add(u32::from_le_bytes([
            key[k + 8],
            key[k + 9],
            key[k + 10],
            key[k + 11],
        ]));
        mix(&mut a, &mut b, &mut c);
        length -= 12;
        k += 12;
    }

    // last block: all the case statements fall through
    // the fall-through chain is order-sensitive; emit the exact sequence
    let g = |idx: usize| -> u32
    {
        if idx < key.len()
        {
            u32::from(key[idx])
        }
        else
        {
            0
        }
    };
    let mut rem = length;
    let mut acc_a = 0u32;
    let mut acc_b = 0u32;
    let mut acc_c = 0u32;
    if rem >= 12
    {
        acc_c |= g(k + 11) << 24;
        rem = 11;
    }
    if rem >= 11
    {
        acc_c |= g(k + 10) << 16;
        rem = 10;
    }
    if rem >= 10
    {
        acc_c |= g(k + 9) << 8;
        rem = 9;
    }
    if rem >= 9
    {
        acc_c |= g(k + 8);
        rem = 8;
    }
    if rem >= 8
    {
        acc_b |= g(k + 7) << 24;
        rem = 7;
    }
    if rem >= 7
    {
        acc_b |= g(k + 6) << 16;
        rem = 6;
    }
    if rem >= 6
    {
        acc_b |= g(k + 5) << 8;
        rem = 5;
    }
    if rem >= 5
    {
        acc_b |= g(k + 4);
        rem = 4;
    }
    if rem >= 4
    {
        acc_a |= g(k + 3) << 24;
        rem = 3;
    }
    if rem >= 3
    {
        acc_a |= g(k + 2) << 16;
        rem = 2;
    }
    if rem >= 2
    {
        acc_a |= g(k + 1) << 8;
        rem = 1;
    }
    if rem >= 1
    {
        acc_a |= g(k);
    }
    a = a.wrapping_add(acc_a);
    b = b.wrapping_add(acc_b);
    c = c.wrapping_add(acc_c);

    // final(a,b,c)
    c ^= b;
    c = c.wrapping_sub(b.rotate_left(14));
    a ^= c;
    a = a.wrapping_sub(c.rotate_left(11));
    b ^= a;
    b = b.wrapping_sub(a.rotate_left(25));
    c ^= b;
    c = c.wrapping_sub(b.rotate_left(16));
    a ^= c;
    a = a.wrapping_sub(c.rotate_left(4));
    b ^= a;
    b = b.wrapping_sub(a.rotate_left(14));
    c ^= b;
    c = c.wrapping_sub(b.rotate_left(24));
    (c, b)
}

/// the UOP name hash pair for a pack-part path ("build/xxx" style names are
/// hashed lowercased, matching the client's hashlittle2 usage)
pub fn uop_name_hashes(name: &str) -> (u32, u32)
{
    let lower = name.to_ascii_lowercase();
    hashlittle2(lower.as_bytes(), 0, 0)
}

pub fn extract_attr(tag: &str, name: &str) -> Option<String>
{
    let needle = format!("{}=\"", name);
    let at = tag.find(&needle)? + needle.len();
    let rest = &tag[at..];
    let end = rest.find('"')?;
    Some(rest[..end].to_string())
}

pub fn find_attr_span(tag: &str, name: &str) -> Option<Range<usize>>
{
    let needle = format!("{}=\"", name);
    let value_start = tag.find(&needle)? + needle.len();
    let rest = &tag[value_start..];
    let end = rest.find('"')?;
    Some(value_start..value_start + end)
}

// ---------------------------------------------------------------------------
// MythicMFT manifest: whole-file `.uop` transfers the proxy normalizes
// ---------------------------------------------------------------------------

/// collects the blob tails of loose whole-file `.uop` transfers declared in
/// a MythicMFT manifest text ("<files><f n="X.uop" .../>"), as
/// "<manifest rpath>/unpacked/<ph><sh>" - the same tails the files route
/// serves and the fetch engine requests
pub fn manifest_loose_uop_tails(text: &str, manifest_rpath: &str) -> Vec<String>
{
    let mut out = Vec::new();
    for cap in f_tag_regex().find_iter(text)
    {
        let tag = cap.as_str();
        if let Some(n) = extract_attr(tag, "n")
        {
            if n.to_ascii_lowercase().ends_with(".uop")
            {
                let (ph, sh) = uop_name_hashes(&n);
                out.push(format!(
                    "{}/unpacked/{:08x}{:08x}",
                    manifest_rpath, ph, sh
                ));
            }
        }
    }
    out
}

static F_TAG: OnceLock<regex::Regex> = OnceLock::new();

fn f_tag_regex() -> &'static regex::Regex
{
    F_TAG.get_or_init(|| regex::Regex::new("<f\\s[^>]*>").unwrap())
}

/// rewrites one whole-file manifest entry to the normalized (all blocks
/// stored, transferred uncompressed) shape: ct=0, cl=ul=size
/// the client parses ul/cl with Hex64Attribute: sizes in MythicMFT are hex
/// strings. Sets the attribute by replacement when present, insertion (after
/// the n attribute, which every entry carries) otherwise - plain transfers
/// like gumpartLegacyMUL.uop ship without ct/cl at all. The replacement span
/// is the FULL attribute (name="value"): replacing just the value span with
/// a full attribute mangles the tag (ul="ul=\"...\"").
fn set_attr_hex(tag: &mut String, name: &str, value: u64)
{
    let encoded = format!("{value:x}");
    let needle = format!("{name}=\"");
    if let Some(at) = tag.find(&needle)
    {
        let value_start = at + needle.len();
        let rest = &tag[value_start..];
        let end = rest.find('"').expect("unterminated manifest attribute");
        let span = at..value_start + end + 1;
        tag.replace_range(span, &format!("{name}=\"{encoded}\""));
        return;
    }
    if let Some(n_span) = find_attr_span(tag, "n")
    {
        let at = n_span.end + 1; // past the closing quote of n="..."
        tag.insert_str(at, &format!(" {name}=\"{encoded}\""));
    }
}

pub fn rewrite_manifest_entry(text: &str, name: &str, size: u64) -> Option<String>
{
    for m in f_tag_regex().find_iter(text)
    {
        let tag = m.as_str();
        if extract_attr(tag, "n").as_deref() != Some(name)
        {
            continue;
        }
        let mut new = tag.to_string();
        set_attr_hex(&mut new, "ct", 0);
        set_attr_hex(&mut new, "cl", size);
        set_attr_hex(&mut new, "ul", size);
        return Some(text.replacen(tag, &new, 1));
    }
    None
}

/// applies normalized sizes to the whole-file `.uop` entries of a manifest
/// text: ct=0 and ul=cl=size for every entry whose blob tail has a known
/// normalized size. Entries without a known size pass through unchanged
/// (the client fetches them as shipped).
pub fn rewrite_manifest_uop_entries(text: &str, sizes: &HashMap<String, u64>) -> Vec<u8>
{
    let mut out = String::with_capacity(text.len());
    let mut last = 0usize;
    for m in f_tag_regex().find_iter(text)
    {
        let tag = m.as_str();
        let name = match extract_attr(tag, "n")
        {
            Some(n) if n.to_ascii_lowercase().ends_with(".uop") => n,
            _ => continue,
        };
        let (ph, sh) = uop_name_hashes(&name);
        // the manifest rpath is already part of the text's urls; rebuild the
        // tail from the tag's surrounding... entries only carry ph/sh, the
        // tail's rpath segment is the manifest rpath - recompute via lookup
        // by matching the hash suffix against known tails
        let hash_suffix = format!("{:08x}{:08x}", ph, sh);
        let size = sizes
            .iter()
            .find(|(tail, _)| tail.ends_with(&hash_suffix))
            .map(|(_, s)| *s);
        if let Some(size) = size
        {
            let mut new = tag.to_string();
            set_attr_hex(&mut new, "ct", 0);
            set_attr_hex(&mut new, "cl", size);
            set_attr_hex(&mut new, "ul", size);
            out.push_str(&text[last..m.start()]);
            out.push_str(&new);
            last = m.end();
        }
    }
    out.push_str(&text[last..]);
    out.into_bytes()
}
