// AGPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

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
    uint32_t Checksum = 0; // crc32 of UopFileMetadata
    uint16_t Flags = 0;    // Compression type (0 - none, 1 - zlib)
};
static_assert(sizeof(UopFileEntry) == 34, "Invalid UopFileEntry size");

struct UopFileMetadata
{
    uint16_t Type = 0;
    uint16_t Size = 0;
};
static_assert(sizeof(UopFileMetadata) == 4, "Invalid UopFileMetadata size");

struct UopFileMetadata3
{
    uint16_t Type = 0;
    uint16_t Size = 0;
    uint64_t Timestamp = 0;
};
static_assert(sizeof(UopFileMetadata3) == 12, "Invalid UopFileMetadata3 size");

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
    uint16_t AnimId = 0;
};

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
