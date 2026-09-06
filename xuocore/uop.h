// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <stdint.h>
#include <common/str.h>

#define UOP_HASH(ph, sh) static_cast<uint64_t>(((uint64_t(ph) << 32) | sh))
#define UOP_HASH_PH(hash) static_cast<uint32_t>(hash >> 32)
#define UOP_HASH_SH(hash) static_cast<uint32_t>(hash & 0xffffffff)
#define MYP_MAGIC 0x0050594D // 'MYP\0' MypArchive / Mythic Package

#pragma pack(push, 1)

struct UopHeader
{
    uint32_t Magic = 0;
    uint32_t Version = 0;
    uint32_t Signature = 0;
    uint64_t SectionOffset = 0;
    uint32_t FileCapacity = 0;
    uint32_t FileCount = 0;
    uint32_t SectionCount = 0;
    uint32_t Unk1 = 0; // SectionCount dup?
    uint32_t Unk2 = 0;
};
static_assert(sizeof(UopHeader) == 40, "Invalid UopHeader size");

struct UopSection
{
    uint32_t FileCount = 0;
    uint64_t NextSection = 0;
};
static_assert(sizeof(UopSection) == 12, "Invalid UopSection size");

struct UopFileEntry
{
    uint64_t Offset = 0;
    uint32_t MetadataSize = 0;
    uint32_t CompressedSize = 0;
    uint32_t DecompressedSize = 0;
    uint64_t Hash = 0;     // (primary hash(ph)|secondary hash(sh))
    uint32_t Checksum = 0; // adler32 of the metadata block for type 3 / version 4 archives
                           // (verified against tileart.uop); for type 4 / version 5 archives
                           // it is a provenance value from the patch manifest ("rh" attribute)
                           // and cannot be recomputed from the stored bytes
    uint16_t Flags = 0;    // Compression type (0 - none, 1 - zlib)
};
static_assert(sizeof(UopFileEntry) == 34, "Invalid UopFileEntry size");

// Metadata stored at UopFileEntry::Offset, before the data block.
// The client (MypArchive::GetMetadataBlock @0x429053 / mapped variant @0x5964b0)
// treats it as a chain of blocks: { u16 Type; u16 Size; uint8_t Payload[Size]; }
// - Size counts the payload ONLY (the 4-byte header is extra); the client walks
//   the chain with ptr += 4 + Size
// - in practice every shipped file carries a single block, so
//   MetadataSize == sizeof(UopFileMetadata) + Size
// - on big-endian archives both u16s are byte swapped (client flag at MypArchive+0x5C)
struct UopFileMetadata
{
    uint16_t Type = 0; // 3 = timestamp, 4 = RSA signature
    uint16_t Size = 0; // payload size in bytes, excluding this 4-byte header
};
static_assert(sizeof(UopFileMetadata) == 4, "Invalid UopFileMetadata size");

struct UopFileMetadata3
{
    uint16_t Type = 0;
    uint16_t Size = 0;
    uint64_t Timestamp = 0; // Windows FILETIME (100ns units since 1601-01-01)
};
static_assert(sizeof(UopFileMetadata3) == 12, "Invalid UopFileMetadata3 size");

// Type 4 payload: RSA signature over the decompressed content, produced by the
// patch system. NOTE: the two u16 fields are stored BIG-ENDIAN (same quirk as
// the .meta/.mft signature records), unlike the block header above.
// EM = Sig^65537 mod N (Mythic launcher key id 21) is
// 33x00 || 32xFF || SHA256(decompressed content) || 32xFF
struct UopFileMetadata4
{
    uint16_t SigType = 0; // key id, big-endian in file (21 = Mythic launcher RSA key)
    uint16_t SigSize = 0; // signature length, big-endian in file (128 = RSA-1024)
    //uint8_t SigData[SigSize]; // follows immediately after
};
static_assert(sizeof(UopFileMetadata4) == 4, "Invalid UopFileMetadata4 size");

struct UopAnimationHeader
{
    // TODO: further invastigate the format for uncertain and unknown fields
    uint32_t Format = 0;  // uncertain
    uint32_t Version = 0; // uncertain
    uint32_t DecompressedSize = 0;
    uint32_t AnimationId = 0;
    uint32_t Unk1 = 0;
    uint32_t Unk2 = 0;
    int16_t Unk3 = 0;
    int16_t Unk4 = 0;
    uint32_t HeaderSize = 0; // uncertain
    uint32_t FrameCount = 0;
    uint32_t Offset = 0;
};
static_assert(sizeof(UopAnimationHeader) == 40, "Invalid UopAnimationHeader size");

struct UopAnimationFrame
{
    // TODO: further invastigate the format for uncertain and unknown fields
    uint8_t *DataStart = nullptr;
    uint16_t GroupId = 0;
    uint16_t FrameId = 0;
    uint32_t Unk1 = 0;
    uint32_t Unk2 = 0;
    uint32_t PixelDataOffset = 0;
};
static_assert(sizeof(UopAnimationHeader) == 40, "Invalid UopAnimationFrame size");

struct UopAnimationSequence
{
    uint16_t Graphic = 0;
    uint16_t Unk0 = 0;
    uint64_t Unk1 = 0;
    uint64_t Unk2 = 0;
    uint64_t Unk3 = 0;
    uint64_t Unk4 = 0;
    uint64_t Unk5 = 0;
    uint64_t Unk6 = 0;
    uint32_t Replaces = 0;
};
static_assert(sizeof(UopAnimationSequence) == 56, "Invalid UopAnimationSequence size");

struct UopAnimationSequenceReplacement
{
    uint32_t Group = 0;
    uint32_t FrameCount = 0;
    uint32_t NewGroup = 0;
    uint8_t Unk[60];
};
static_assert(
    sizeof(UopAnimationSequenceReplacement) == 72, "Invalid UopAnimationSequenceReplacement size");

// string_dictionary.uop
struct UopDictionary
{
    uint32_t Unk1;
    uint32_t Unk2;
    uint32_t Count;
    uint32_t Unk3;
};
static_assert(sizeof(UopDictionary) == 16, "Invalid UopDictionary size");

// tileart.uop
struct UopTileArtImage
{
    uint32_t StartX;
    uint32_t StartY;
    uint32_t EndX;
    uint32_t EndY;
    uint32_t OffsetX;
    uint32_t OffsetY;
};

enum TileArtPropertyId : uint8_t
{
    Weight = 0,
    Quality = 1,
    Quantity = 2,
    Height = 3,
    Value = 4,
    AcVc = 5,
    Slot = 6,
    off_C8 = 7,
    Appearance = 8,
    Race = 9,
    Gender = 10,
    Paperdoll = 11
};

enum EffectsId : uint32_t
{
    Effect00 = 0,
    Effect01 = 1,
    Effect02 = 2,
    Effect07 = 7,
    Effect10 = 10,
    Effect11 = 11,
    Effect12 = 12,
    Effect15 = 15,
    Effect16 = 16,
    Effect17 = 17
};

// FIXME: Tile Flags enum
// FIXME: Tile Flags2 enum

struct UopTileProperty
{
    TileArtPropertyId PropertyId;
    uint32_t Value;
};

struct UopTileStack
{
    int32_t Amount;
    int32_t AmountId;
};

struct UopTileArtDefinition
{
    uint16_t Type;
    uint32_t Name;
    uint32_t Id;
    uint8_t unk;
    uint8_t unk7;
    float unk2;
    float unk3;
    int32_t zero;
    int32_t OldId;
    int32_t unk6;
    int32_t unk_type;
    uint8_t unk8;
    int32_t unk9;
    int32_t unk10;
    float unk11;
    float unk12;
    int32_t unk13;
    uint64_t Flags;
    uint64_t Flags2;
    int32_t unk16;
    UopTileArtImage Image1;
    UopTileArtImage Image2;
    //uint8_t PropertyCount1;
    //UopTileProperty Properties1[PropertyCount1];
    //uint8_t PropertyCount2;
    //UopTileProperty Properties2[PropertyCount2];
    //uint32_t StackCount;
    //UopTileStack Stacks[StackCount];
    //AnimationAppearance
    //SittingAnimation
    //RadarCol
    //Texture[4]
    //uint8_t EffectCount;
    //Effect EffectsId+Data[EffectCount];
};

#pragma pack(pop)

// Hardcoded asset hashes/names

enum Asset : uint64_t
{
    // MainMisc.uop
    // meta = 0x0c / 12 bytes
    WearableLayers = 0x021c35a18c8bc953,    // xml
    CommandsTranslate = 0x0891f809004d8081, // bin
    AssetMap = 0x6b1786d68753f504,          // xml
    TileAnimData = 0xc0165b63d153b1dd,      // bin
    // .uop
    // meta = 0x0c / 12 bytes
    Strings = 0x00e53aa29612dc85, // bin
};

void uop_populate_asset_names();
const astr_t &uop_asset_name(const char *package, uint64_t hash, uint32_t *out_id);
