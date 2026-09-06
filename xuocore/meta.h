// AGPLv3 License
// Copyright (c) 2026 The crossuo authors
//
// Verification of Ultima Online launcher ".meta" signature files and
// MythicMFT manifest <sig> blocks, reverse engineered from UO.exe.
//
// .meta file layout:
//   [u16 LE version = 4][u16 LE payload_len][u16 BE key_id][u16 BE block_len][RSA block]
// MythicMFT <sig id="key_id" v="base64"> layout:
//   [u16 BE block_len][RSA block] (base64 decoded, key_id comes from the attribute)
//
// The RSA block is a big-endian integer s. Verification recovers
//   em = s^65537 mod N
// which is 129 bytes (N is a 1026-bit modulus) laid out as:
//   em[0..32]   = 0x00
//   em[33..64]  = hash slot A (0xff filled when unused)
//   em[65..96]  = SHA-256 of the file content (uncompressed)
//   em[97..128] = hash slot C (0xff filled when unused)
//
// The content that gets hashed is the uncompressed file data, which for
// compressed entries (compression_type == 1) is the zlib decompressed buffer.

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace meta
{
// SHA-256 of data into out (32 bytes)
void sha256(const uint8_t *data, size_t len, uint8_t out[32]);

// Verifies a full ".meta" file against a precomputed SHA-256 of the content
bool verify_meta(const uint8_t *meta, size_t meta_len, const uint8_t sha256_digest[32]);

// Verifies a raw RSA signature block (as found in MythicMFT <sig> values)
// against a precomputed SHA-256 of the content
bool verify_sig_block(const uint8_t *block, size_t block_len, const uint8_t sha256_digest[32]);

} // namespace meta
