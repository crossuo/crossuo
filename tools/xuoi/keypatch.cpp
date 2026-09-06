// AGPLv3 License
// Copyright (c) 2026 The crossuo authors

#include "keypatch.h"

#include <stdio.h>
#include <string.h>

namespace keypatch
{
// Constructor layout (offsets relative to the constructor start, which sits at
// VA 0x43e240 in every known build). Derived from the disassembly of the
// stack-store block that builds the 145-byte key blob:
//
//   +0x000  sub  $0x98,%esp
//   +0x006  mov  <__security_cookie>,%eax        (cookie address varies per build)
//   +0x00b  xor  %esp,%eax
//   +0x00d  mov  %eax,0x94(%esp)
//   +0x014  mov  $0x12,%dl
//   +0x016  push %esi                            ; preserve
//   +0x017  mov  %ecx,%esi                       ; esi = this
//   +0x019  xor  %al,%al
//   +0x01b  .. hundreds of movb/movl stores ..   ; blob[0] at +0x03b (anchor)
//   +0x1a8  lea  0x4(%esp),%eax                  ; blob argument
//   +0x1ac  push $0x91                           ; blob length
//   +0x1b1  push %eax
//   +0x1b2  .. more stores ..
//   +0x343  call <CryptoKey_CreateFromBlob>
//   +0x348  add  $0x8,%esp                       ; execution resumes here
//
// Replacement code written at +0x01b (esp == blob_base - 4 at this point):
//
//   cld
//   push %esi
//   push %edi
//   mov  $<blob_va>,%esi
//   lea  0xc(%esp),%edi                          ; blob_base
//   mov  $0x91,%ecx
//   rep  movsb
//   pop  %edi
//   pop  %esi
//   lea  0x4(%esp),%eax
//   push $0x91
//   push %eax
//   call <CryptoKey_CreateFromBlob>              ; relocated
//   jmp  +0x347                                  ; original resume point
//   +0x044  <145 byte key blob>
//   +0x0d3  .. int3 padding ..                   ; dead store block
namespace
{
constexpr size_t OFF_CTOR_SUB = 0x000;
constexpr size_t OFF_CTOR_XOR_ESP = 0x00b;
constexpr size_t OFF_CTOR_MOV_DL = 0x014;
constexpr size_t OFF_CTOR_PUSH_ESI = 0x016;
constexpr size_t OFF_CTOR_ANCHOR = 0x03b;
constexpr size_t OFF_CTOR_LEA = 0x1a8;
constexpr size_t OFF_CTOR_CALL = 0x343;
constexpr size_t OFF_CTOR_RESUME = 0x348;
constexpr size_t OFF_PATCH_CODE = 0x01b;
constexpr size_t OFF_PATCH_BLOB = 0x044;
constexpr size_t OFF_PATCH_TAIL = 0x0d5; // blob end (0x44 + 0x91) -> int3 padding

constexpr size_t ANCHOR_LEN = 5;

bool check_bytes(const uint8_t *image, size_t size, size_t off, const char *hex, const char *what, char *err, size_t err_len)
{
    const size_t len = strlen(hex) / 2;
    if (off + len > size)
    {
        snprintf(err, err_len, "image too small at %s", what);
        return false;
    }
    for (size_t i = 0; i < len; i++)
    {
        unsigned byte = 0;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1 || image[off + i] != uint8_t(byte))
        {
            snprintf(err, err_len, "unexpected bytes at %s (offset +0x%zx)", what, off);
            return false;
        }
    }
    return true;
}

struct PeText
{
    uint32_t image_base = 0;
    size_t raw_off = 0;
    size_t raw_size = 0;
    uint32_t va = 0;
};

bool parse_pe_text(const uint8_t *image, size_t size, PeText &out, char *err, size_t err_len)
{
    if (size < 0x40 || memcmp(image, "MZ", 2) != 0)
    {
        snprintf(err, err_len, "not an MZ executable");
        return false;
    }
    uint32_t pe_off = 0;
    memcpy(&pe_off, image + 0x3c, 4);
    if (!pe_off || pe_off + 0x18 > size || memcmp(image + pe_off, "PE\0\0", 4) != 0)
    {
        snprintf(err, err_len, "not a PE image");
        return false;
    }
    const uint16_t opt_size = uint16_t(image[pe_off + 20] | (image[pe_off + 21] << 8));
    const uint16_t num_sections = uint16_t(image[pe_off + 6] | (image[pe_off + 7] << 8));
    const uint32_t opt_off = pe_off + 24;
    if (opt_off + opt_size > size || image[opt_off] != 0x0b) // PE32 magic
    {
        snprintf(err, err_len, "unsupported PE optional header");
        return false;
    }
    memcpy(&out.image_base, image + opt_off + 28, 4);
    const size_t sec_off = opt_off + opt_size;
    for (uint16_t i = 0; i < num_sections; i++)
    {
        const size_t s = sec_off + 40 * size_t(i);
        if (s + 40 > size)
            break;
        if (memcmp(image + s, ".text", 5) != 0)
            continue;
        uint32_t vsize, vaddr, rsize, rptr;
        memcpy(&vsize, image + s + 8, 4);
        memcpy(&vaddr, image + s + 12, 4);
        memcpy(&rsize, image + s + 16, 4);
        memcpy(&rptr, image + s + 20, 4);
        out.va = out.image_base + vaddr;
        out.raw_off = rptr;
        out.raw_size = rsize;
        return true;
    }
    snprintf(err, err_len, "no .text section found");
    return false;
}

} // namespace

bool build_key_blob(
    const uint8_t *modulus, size_t modulus_len, const uint8_t *exponent, size_t exponent_len,
    uint8_t out[KEY_BLOB_LEN])
{
    if (!modulus || !exponent || modulus_len == 0 || modulus_len > KEY_MODULUS_LEN || exponent_len == 0 || exponent_len > 4)
        return false;

    memset(out, 0, KEY_BLOB_LEN);
    out[0] = 0x91;          // total length
    out[1] = 0x00;
    out[2] = 0x00;          // key type: public
    out[3] = 0x00;
    out[4] = 0x01;          // version
    out[5] = KEY_MODULUS_LEN; // modulus length, u32 LE
    out[138] = uint8_t(exponent_len);
    memcpy(out + 9 + (KEY_MODULUS_LEN - modulus_len), modulus, modulus_len);
    memcpy(out + 142 + (3 - exponent_len), exponent, exponent_len);
    return true;
}

bool find_key_ctor(const uint8_t *image, size_t size, size_t *offset, uint32_t *va, char *err, size_t err_len)
{
    PeText text;
    if (!parse_pe_text(image, size, text, err, err_len))
        return false;

    // the anchor is the first blob byte store: movb $0x91,0x4(%esp)
    static const uint8_t anchor[ANCHOR_LEN] = { 0xc6, 0x44, 0x24, 0x04, 0x91 };
    for (size_t pos = text.raw_off; pos + ANCHOR_LEN <= text.raw_off + text.raw_size; pos++)
    {
        if (memcmp(image + pos, anchor, ANCHOR_LEN) != 0)
            continue;
        if (pos < OFF_CTOR_ANCHOR)
            continue;
        const size_t f = pos - OFF_CTOR_ANCHOR;
        if (!check_bytes(image, size, f + OFF_CTOR_SUB, "81ec98000000", "ctor sub", err, err_len) ||
            !check_bytes(image, size, f + OFF_CTOR_XOR_ESP, "33c4", "ctor xor", err, err_len) ||
            !check_bytes(image, size, f + OFF_CTOR_MOV_DL, "b2", "ctor mov dl", err, err_len) ||
            !check_bytes(image, size, f + OFF_CTOR_PUSH_ESI, "568bf132c0", "ctor prologue", err, err_len) ||
            !check_bytes(image, size, f + OFF_CTOR_LEA, "8d442404689100000050", "ctor lea/args", err, err_len) ||
            !check_bytes(image, size, f + OFF_CTOR_CALL, "e8", "ctor call", err, err_len) ||
            !check_bytes(image, size, f + OFF_CTOR_RESUME, "83c408", "ctor resume", err, err_len))
            continue;
        *offset = f;
        *va = text.va + uint32_t(f - text.raw_off);
        return true;
    }
    snprintf(err, err_len, "key constructor not found (unknown launcher build?)");
    return false;
}

bool read_patched_key(const uint8_t *image, size_t size, uint8_t out[KEY_BLOB_LEN], char *err, size_t err_len)
{
    // after patching, the blob is announced by its own header: 0x91,0,0,0,1,0x81
    static const uint8_t blob_magic[] = { 0x91, 0x00, 0x00, 0x00, 0x01, 0x81 };
    for (size_t pos = 0; pos + KEY_BLOB_LEN <= size; pos++)
    {
        if (memcmp(image + pos, blob_magic, sizeof(blob_magic)) != 0)
            continue;
        if (pos < OFF_PATCH_BLOB)
            break;
        const size_t f = pos - OFF_PATCH_BLOB;
        // must be preceded by our copy stub (cld; push esi; push edi; mov $...)
        if (image[f + OFF_PATCH_CODE] != 0xfc || image[f + OFF_PATCH_CODE + 1] != 0x56 || image[f + OFF_PATCH_CODE + 2] != 0x57)
            continue;
        memcpy(out, image + pos, KEY_BLOB_LEN);
        return true;
    }
    snprintf(err, err_len, "patched key blob not found (image not patched?)");
    return false;
}

bool patch_image(uint8_t *image, size_t size, const uint8_t blob[KEY_BLOB_LEN], char *err, size_t err_len)
{
    size_t f = 0;
    uint32_t f_va = 0;
    if (!find_key_ctor(image, size, &f, &f_va, err, err_len))
        return false;

    // relocate the parser call into our stub
    int32_t call_rel = 0;
    memcpy(&call_rel, image + f + OFF_CTOR_CALL + 1, 4);
    const int32_t new_call_rel = call_rel + int32_t(OFF_CTOR_CALL - 0x3a);
    const uint32_t blob_va = f_va + OFF_PATCH_BLOB;

    uint8_t code[OFF_PATCH_BLOB - OFF_PATCH_CODE] = {};
    size_t p = 0;
    code[p++] = 0xfc;                         // cld
    code[p++] = 0x56;                         // push %esi
    code[p++] = 0x57;                         // push %edi
    code[p++] = 0xbe;                         // mov $blob_va,%esi
    memcpy(code + p, &blob_va, 4), p += 4;
    code[p++] = 0x8d, code[p++] = 0x7c, code[p++] = 0x24, code[p++] = 0x0c; // lea 0xc(%esp),%edi
    code[p++] = 0xb9, code[p++] = 0x91, code[p++] = 0x00, code[p++] = 0x00, code[p++] = 0x00; // mov $0x91,%ecx
    code[p++] = 0xf3, code[p++] = 0xa4;       // rep movsb
    code[p++] = 0x5f;                         // pop %edi
    code[p++] = 0x5e;                         // pop %esi
    code[p++] = 0x8d, code[p++] = 0x44, code[p++] = 0x24, code[p++] = 0x04; // lea 0x4(%esp),%eax
    code[p++] = 0x68, code[p++] = 0x91, code[p++] = 0x00, code[p++] = 0x00, code[p++] = 0x00; // push $0x91
    code[p++] = 0x50;                         // push %eax
    code[p++] = 0xe8;                         // call CryptoKey_CreateFromBlob
    memcpy(code + p, &new_call_rel, 4), p += 4;
    code[p++] = 0xe9;                         // jmp resume
    const int32_t jmp_rel = int32_t(OFF_CTOR_RESUME - 0x44);
    memcpy(code + p, &jmp_rel, 4), p += 4;
    static_assert(OFF_PATCH_BLOB == 0x44, "code layout size mismatch");
    if (p != sizeof(code))
    {
        snprintf(err, err_len, "internal code layout error");
        return false;
    }

    memcpy(image + f + OFF_PATCH_CODE, code, sizeof(code));
    memcpy(image + f + OFF_PATCH_BLOB, blob, KEY_BLOB_LEN);
    memset(image + f + OFF_PATCH_TAIL, 0xcc, OFF_CTOR_RESUME - OFF_PATCH_TAIL);
    return true;
}

bool patch_file(const char *path, const uint8_t blob[KEY_BLOB_LEN], char *err, size_t err_len)
{
    FILE *file = fopen(path, "rb");
    if (!file)
    {
        snprintf(err, err_len, "cannot open %s", path);
        return false;
    }
    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0)
    {
        fclose(file);
        snprintf(err, err_len, "empty file %s", path);
        return false;
    }
    uint8_t *image = new uint8_t[size];
    if (fread(image, 1, size, file) != size_t(size))
    {
        delete[] image;
        fclose(file);
        snprintf(err, err_len, "short read on %s", path);
        return false;
    }
    fclose(file);

    bool ok = patch_image(image, size_t(size), blob, err, err_len);
    if (ok)
    {
        file = fopen(path, "wb");
        if (!file || fwrite(image, 1, size, file) != size_t(size))
        {
            snprintf(err, err_len, "cannot write %s", path);
            ok = false;
        }
        if (file)
            fclose(file);
    }
    delete[] image;
    return ok;
}

} // namespace keypatch
