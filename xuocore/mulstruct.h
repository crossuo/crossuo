// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <stdint.h>
#include <vector> // FIXME

// References:
// https://uo.stratics.com/heptazane/fileformats.shtml

#pragma pack(push, 1)

#define MYP_MAGIC 0x0050594D // 'MYP\0' MypArchive / Mythic Package

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
    uint64_t Hash = 0;
    uint32_t Checksum = 0; // crc32 of UopFileMetadata
    uint16_t Flags = 0;    // Compression type (0 - none, 1 - zlib)
};
static_assert(sizeof(UopFileEntry) == 34, "Invalid UopFileEntry size");

struct UopFileMetadata
{
    uint16_t Type = 0;
    uint16_t Size = 0;
    //uint16_t SigType;
    //uint16_t SigSize;
    //uint32_t DataPtr;
};
static_assert(sizeof(UopFileMetadata) == 4, "Invalid UopFileMetadata size");

struct UopFileMetadata3
{
    uint32_t Unk = 0;
};
static_assert(sizeof(UopFileMetadata3) == 4, "Invalid UopFileMetadata3 size");

struct UopFileMetadata4
{
    uint16_t SigType = 0;
    uint16_t SigSize = 0;
    //uint8_t *Data[SigSize];
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

// was ANIMATION_DIMENSIONS
struct AnimationFrameInfo
{
    //  uint16_t Palette[512];
    int16_t CenterX = 0;
    int16_t CenterY = 0;
    int16_t Width = 0;
    int16_t Height = 0;
};
static_assert(sizeof(AnimationFrameInfo) == 8, "Invalid Animation Frame Info size");

struct AnimationSelector
{
    uint8_t Group = 0;
    uint8_t Direction = 0;
    uint16_t Graphic = 0;
};
static_assert(sizeof(AnimationSelector) == 4, "Invalid Animation Selector size");

struct VERDATA_HEADER
{
    uint32_t FileID;
    uint32_t BlockID;
    uint32_t Position;
    uint32_t Size;
    uint32_t GumpData;
};
static_assert(sizeof(VERDATA_HEADER) == 20, "Invalid VERDATA_HEADER size");

struct IdxImageData
{
    uint16_t Height;
    uint16_t Width;
};
struct IdxSoundData
{
    uint16_t Index;
    uint16_t Reserved;
};

struct IndexBlock
{
    uint32_t Position;
    uint32_t Size;
    union {
        IdxImageData GumpData;
        IdxImageData LightData;
        IdxSoundData SoundData;
        uint32_t Unknown; // Map, Sta, Multi, Skills, Art, Anim, Texture
    };
};
static_assert(sizeof(IndexBlock) == 12, "Invalid IndexBlock size");

typedef IndexBlock GumpIdxBlock;
typedef IndexBlock LightIdxBlock;
typedef IndexBlock SoundIdxBlock;
typedef IndexBlock TexIdxBlock;
typedef IndexBlock AnimIdxBlock;
typedef IndexBlock ArtIdxBlock;
typedef IndexBlock SkillIdxBlock;
typedef IndexBlock MultiIdxBlock;
typedef IndexBlock StaIdxBlock;

struct MAP_CELLS
{
    uint16_t TileID;
    char Z;
};
static_assert(sizeof(MAP_CELLS) == 3, "Invalid MAP_CELLS size");

struct MAP_BLOCK
{
    uint32_t Header;
    MAP_CELLS Cells[64];
};
static_assert(sizeof(MAP_BLOCK) == 196, "Invalid MAP_BLOCK size");

struct RADAR_MAP_CELLS
{
    uint16_t Graphic;
    char Z;
    char IsLand;
};
static_assert(sizeof(RADAR_MAP_CELLS) == 4, "Invalid RADAR_MAP_CELLS size");

struct RADAR_MAP_BLOCK
{
    RADAR_MAP_CELLS Cells[8][8];
};
static_assert(sizeof(RADAR_MAP_BLOCK) == 256, "Invalid RADAR_MAP_BLOCK size");

struct STATICS_BLOCK
{
    uint16_t Color;
    uint8_t X;
    uint8_t Y;
    char Z;
    uint16_t Hue;
};
static_assert(sizeof(STATICS_BLOCK) == 7, "Invalid STATICS_BLOCK size");

struct MulLandTile1
{
    uint32_t Flags;
    uint16_t TexID;
    char Name[20];
};
static_assert(sizeof(MulLandTile1) == 26, "Invalid MulLandTile1 (<7.0.9) size");

struct MulLandTileGroup1
{
    uint32_t Header;
    MulLandTile1 Tiles[32];
};
static_assert(sizeof(MulLandTileGroup1) == 836, "Invalid MulLandTileGroup1 (<7.0.9) size");

struct MulStaticTile1
{
    uint32_t Flags;
    uint8_t Weight;
    uint8_t Layer;
    uint32_t Count;
    uint16_t AnimID;
    uint16_t Hue;
    uint16_t LightIndex;
    uint8_t Height;
    char Name[20];
};
static_assert(sizeof(MulStaticTile1) == 37, "Invalid MulStaticTile1 (<7.0.9) size");

struct MulStaticTileGroup1
{
    uint32_t Header;
    MulStaticTile1 Tiles[32];
};
static_assert(sizeof(MulStaticTileGroup1) == 1188, "Invalid MulStaticTileGroup1 (<7.0.9) size");

struct MulLandTile2
{
    uint64_t Flags;
    uint16_t TexID;
    char Name[20];
};
static_assert(sizeof(MulLandTile2) == 30, "Invalid MulLandTile2 (>=7.0.9) size");

struct MulLandTileGroup2
{
    uint32_t Header;
    MulLandTile2 Tiles[32];
};
static_assert(sizeof(MulLandTileGroup2) == 964, "Invalid MulLandTileGroup2 (>=7.0.9) size");

struct MulStaticTile2
{
    uint64_t Flags;
    uint8_t Weight;
    uint8_t Layer;
    uint32_t Count;
    uint16_t AnimID;
    uint16_t Hue;
    uint16_t LightIndex;
    uint8_t Height;
    char Name[20];
};
static_assert(sizeof(MulStaticTile2) == 41, "Invalid MulStaticTile2 (>=7.0.9) size");

struct MulStaticTileGroup2
{
    uint32_t Header; // TODO: Check what this and other headers are (crc32?)
    MulStaticTile2 Tiles[32];
};
static_assert(sizeof(MulStaticTileGroup2) == 1316, "Invalid MulStaticTileGroup2 (>=7.0.9) size");

struct MULTI_BLOCK
{
    uint16_t ID;
    int16_t X;
    int16_t Y;
    int16_t Z;
    uint32_t Flags;
};
static_assert(sizeof(MULTI_BLOCK) == 12, "Invalid MULTI_BLOCK size");

struct MULTI_BLOCK_NEW
{
    uint16_t ID;
    int16_t X;
    int16_t Y;
    int16_t Z;
    uint32_t Flags;
    int32_t Unknown;
};
static_assert(sizeof(MULTI_BLOCK_NEW) == 16, "Invalid MULTI_BLOCK_NEW size");

struct HUES_BLOCK
{
    uint16_t ColorTable[32];
    uint16_t TableStart;
    uint16_t TableEnd;
    char Name[20];
};
static_assert(sizeof(RADAR_MAP_CELLS) == 4, "Invalid RADAR_MAP_CELLS size");

struct HUES_GROUP
{
    uint32_t Header;
    HUES_BLOCK Entries[8];
};
static_assert(sizeof(HUES_BLOCK) == 88, "Invalid HUES_BLOCK size");

struct VERDATA_HUES_BLOCK
{
    uint16_t ColorTable[32];
    uint16_t TableStart;
    uint16_t TableEnd;
    char Name[20];
    uint16_t Unk[32];
};
static_assert(sizeof(VERDATA_HUES_BLOCK) == 152, "Invalid VERDATA_HUES_BLOCK size");

struct VERDATA_HUES_GROUP
{
    uint32_t Header;
    VERDATA_HUES_BLOCK Entries[8];
};
static_assert(sizeof(VERDATA_HUES_GROUP) == 1220, "Invalid VERDATA_HUES_GROUP size");

struct GUMP_BLOCK
{
    uint16_t Value;
    uint16_t Run;
};
static_assert(sizeof(GUMP_BLOCK) == 4, "Invalid GUMP_BLOCK size");

/*
struct SKILLS_BLOCK
{
    uint8_t Button;
    char *SkillName;
};
static_assert(sizeof(SKILLS_BLOCK) == 0, "Invalid SKILLS_BLOCK size");
*/
/*
struct BODYCONV_DATA
{
    int16_t Anim2;
    int16_t Anim3;
    int16_t Anim4;
    int16_t Anim5;

    void Reset()
    {
        Anim2 = -1;
        Anim3 = -1;
        Anim4 = -1;
        Anim5 = -1;
    }
};
static_assert(sizeof(BODYCONV_DATA) == 0, "Invalid BODYCONV_DATA size");
*/
struct ANIM_DATA
{
    char FrameData[64];
    uint8_t Unknown;
    uint8_t FrameCount;
    uint8_t FrameInterval;
    uint8_t FrameStart;
};
static_assert(sizeof(ANIM_DATA) == 68, "Invalid ANIM_DATA size");
/*
struct PALETTE_BLOCK
{
    uint8_t R;
    uint8_t G;
    uint8_t B;
};
static_assert(sizeof(PALETTE_BLOCK) == 0, "Invalid PALETTE_BLOCK size");
*/
#ifndef XUOASSISTANT_LIBRARY

struct FONT_HEADER
{
    uint8_t Width;
    uint8_t Height;
    uint8_t Unknown;
};
static_assert(sizeof(FONT_HEADER) == 3, "Invalid FONT_HEADER size");

struct FONT_CHARACTER
{
    uint8_t Width;
    uint8_t Height;
    uint8_t Unknown;
};
static_assert(sizeof(FONT_CHARACTER) == 3, "Invalid FONT_CHARACTER size");

struct FONT_CHARACTER_DATA
{
    uint8_t Width;
    uint8_t Height;
    std::vector<uint16_t> Data; // FIXME
};
//static_assert(sizeof(FONT_CHARACTER_DATA) == 0, "Invalid FONT_CHARACTER_DATA size");

struct FONT_DATA
{
    uint8_t Header;
    FONT_CHARACTER_DATA Chars[224];
};
//static_assert(sizeof(FONT_DATA) == 0, "Invalid FONT_DATA size");

struct FONT_OBJECT
{
    uint8_t Width;
    uint8_t Height;
    uint32_t Texture; // GLuint
    uint8_t *Data;
};
static_assert(sizeof(FONT_OBJECT) == 14, "Invalid FONT_OBJECT size");

struct UNICODE_FONT_DATA
{
    uint8_t OffsetX;
    uint8_t OffsetY;
    uint8_t Width;
    uint8_t Height;
};
static_assert(sizeof(UNICODE_FONT_DATA) == 4, "Invalid UNICODE_FONT_DATA size");

struct SOUND_BLOCK
{
    char Name[16];
    uint32_t Unknown1;
    uint32_t Unknown2;
    uint32_t Unknown3;
    uint32_t Unknown4;
    //Data;
};
static_assert(sizeof(SOUND_BLOCK) == 32, "Invalid SOUND_BLOCK size");

#endif // XUOASSISTANT_LIBRARY

#pragma pack(pop)
