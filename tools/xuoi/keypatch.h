// AGPLv3 License
// Copyright (c) 2026 The crossuo authors
//
// Replacement of the RSA public key embedded in the UO.exe launcher.
//
// The launcher (UO.exe) verifies every downloaded blob/manifest against RSA
// signatures shipped as ".meta" sidecars using a hard-coded 1026-bit public
// key (key_id 21). The key material is not stored as data: a constructor
// function builds a 145-byte key blob on the stack through hundreds of
// immediate byte stores, then hands it to the key parser.
//
// This patcher rewrites that constructor: the store block is replaced by a
// tiny stub that copies a new 145-byte key blob (embedded in the freed space)
// into the very same stack buffer, preserving registers, stack layout and the
// following parser call. The result is a UO.exe that accepts metadata signed
// with YOUR private key, enabling fully self-hosted install/patch servers.
//
// The constructor layout this relies on is byte-identical in every known
// launcher build from 7.0.45.65 (2009) through 7.0.85.15+ (2020) at VA
// 0x43e240; every assumption is verified before any byte is written and the
// patch is refused otherwise.

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace keypatch
{
static const size_t KEY_MODULUS_LEN = 129; // 1026-bit modulus
static const size_t KEY_BLOB_LEN = 145;    // full launcher key blob

// Builds the 145-byte launcher key blob from raw key components.
// modulus: big-endian, 1..129 bytes (left padded with zeros)
// exponent: big-endian, 1..4 bytes (e.g. 01 00 01 for 65537)
bool build_key_blob(
    const uint8_t *modulus, size_t modulus_len, const uint8_t *exponent, size_t exponent_len,
    uint8_t out[KEY_BLOB_LEN]);

// Locates the key constructor inside a UO.exe image.
// On success returns the constructor file offset and stores its VA in *va.
bool find_key_ctor(const uint8_t *image, size_t size, size_t *offset, uint32_t *va, char *err, size_t err_len);

// Extracts the key blob from an already patched image (blob at ctor+0x44).
bool read_patched_key(const uint8_t *image, size_t size, uint8_t out[KEY_BLOB_LEN], char *err, size_t err_len);

// Patches a UO.exe image in memory (must be writable).
bool patch_image(uint8_t *image, size_t size, const uint8_t blob[KEY_BLOB_LEN], char *err, size_t err_len);

// Convenience wrapper: reads the file, patches and writes it back.
bool patch_file(const char *path, const uint8_t blob[KEY_BLOB_LEN], char *err, size_t err_len);

} // namespace keypatch
