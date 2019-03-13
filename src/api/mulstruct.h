// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <stdint.h>
#include <string>

#pragma pack(push, 1)

#define MYP_MAGIC 0x0050594D

struct UopHeader
{
    uint32_t Magic = 0;
    uint32_t Version = 0;
    uint32_t Signature = 0;
    uint64_t FirstSection = 0;
    uint32_t MaxBlockCount = 0;
    uint32_t FileCount = 0;
    uint32_t Pad1 = 0;
    uint32_t Pad2 = 0;
    uint32_t Pad3 = 0;
};
static_assert(sizeof(UopHeader) == 40, "Invalid UOP Header size");

struct UopBlockSection
{
    uint32_t FileCount = 0;
    uint64_t NextSection = 0;
};
static_assert(sizeof(UopBlockSection) == 12, "Invalid UOP Section size");

struct UopBlockHeader
{
    uint64_t Offset = 0;
    uint32_t HeaderSize = 0;
    uint32_t CompressedSize = 0;
    uint32_t DecompressedSize = 0;
    uint64_t Hash = 0;
    uint32_t Checksum = 0; // crc32 of UopBlockMetadata
    uint16_t Flags = 0;    // Compression type (0 - none, 1 - zlib)
};
static_assert(sizeof(UopBlockHeader) == 34, "Invalid UOP File Block size");

struct UopBlockMetadata
{
    uint16_t Type;
    uint16_t Offset;
    uint64_t Timestamp; // usec
};
static_assert(sizeof(UopBlockMetadata) == 12, "Invalid UOP Metadata size");

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
static_assert(sizeof(UopAnimationHeader) == 40, "Invalid UOP Animation Header size");

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
static_assert(sizeof(UopAnimationHeader) == 40, "Invalid UOP Animation Frame size");

struct VERDATA_HEADER
{
    unsigned int FileID;
    unsigned int BlockID;
    unsigned int Position;
    unsigned int Size;
    unsigned int GumpData;
};
static_assert(sizeof(VERDATA_HEADER) == 20, "Invalid VERDATA_HEADER size");

struct BASE_IDX_BLOCK
{
    unsigned int Position;
    unsigned int Size;
};
static_assert(sizeof(BASE_IDX_BLOCK) == 8, "Invalid BASE_IDX_BLOCK size");

/*
struct MAP_CELLS_EX
{
    unsigned short TileID;
    unsigned short Color;
    char Z;
};
*/

struct MAP_CELLS
{
    unsigned short TileID;
    char Z;
};
static_assert(sizeof(MAP_CELLS) == 3, "Invalid MAP_CELLS size");

struct MAP_BLOCK
{
    unsigned int Header;
    MAP_CELLS Cells[64];
};
static_assert(sizeof(MAP_BLOCK) == 196, "Invalid MAP_BLOCK size");

struct RADAR_MAP_CELLS
{
    unsigned short Graphic;
    char Z;
    char IsLand;
};
static_assert(sizeof(RADAR_MAP_CELLS) == 4, "Invalid RADAR_MAP_CELLS size");

struct RADAR_MAP_BLOCK
{
    RADAR_MAP_CELLS Cells[8][8];
};
static_assert(sizeof(RADAR_MAP_BLOCK) == 256, "Invalid RADAR_MAP_BLOCK size");

struct STAIDX_BLOCK : public BASE_IDX_BLOCK
{
    unsigned int Unknown;
};
static_assert(sizeof(STAIDX_BLOCK) == 12, "Invalid STAIDX_BLOCK size");

struct STATICS_BLOCK
{
    unsigned short Color;
    unsigned char X;
    unsigned char Y;
    char Z;
    unsigned short Hue;
};
static_assert(sizeof(STATICS_BLOCK) == 7, "Invalid STATICS_BLOCK size");

struct LAND_TILES_OLD
{
    unsigned int Flags;
    unsigned short TexID;
    char Name[20];
};
static_assert(sizeof(LAND_TILES_OLD) == 26, "Invalid LAND_TILES_OLD size");

struct LAND_GROUP_OLD
{
    unsigned int Unknown;
    LAND_TILES_OLD Tiles[32];
};
static_assert(sizeof(LAND_GROUP_OLD) == (26 * 32) + 4, "Invalid LAND_GROUP_OLD size");

struct STATIC_TILES_OLD
{
    unsigned int Flags;
    unsigned char Weight;
    unsigned char Layer;
    unsigned int Count;
    unsigned short AnimID;
    unsigned short Hue;
    unsigned short LightIndex;
    unsigned char Height;
    char Name[20];
};
static_assert(sizeof(STATIC_TILES_OLD) == 37, "Invalid STATIC_TILES_OLD size");

struct STATIC_GROUP_OLD
{
    unsigned int Unk;
    STATIC_TILES_OLD Tiles[32];
};
static_assert(sizeof(STATIC_GROUP_OLD) == 37 * 32 + 4, "Invalid STATIC_GROUP_OLD size");

struct LAND_TILES_NEW
{
    uint64_t Flags;
    unsigned short TexID;
    char Name[20];
};
static_assert(sizeof(LAND_TILES_NEW) == 30, "Invalid RADAR_LAND_TILES_NEWMAP_CELLS size");

struct LAND_GROUP_NEW
{
    unsigned int Unknown;
    LAND_TILES_NEW Tiles[32];
};
static_assert(sizeof(LAND_GROUP_NEW) == 964, "Invalid LAND_GROUP_NEW size");

struct STATIC_TILES_NEW
{
    uint64_t Flags;
    unsigned char Weight;
    unsigned char Layer;
    unsigned int Count;
    unsigned short AnimID;
    unsigned short Hue;
    unsigned short LightIndex;
    unsigned char Height;
    char Name[20];
};
static_assert(sizeof(STATIC_TILES_NEW) == 41, "Invalid STATIC_TILES_NEW size");

struct STATIC_GROUP_NEW
{
    unsigned int Unk;
    STATIC_TILES_NEW Tiles[32];
};
static_assert(sizeof(STATIC_GROUP_NEW) == (32 * 41) + 4, "Invalid STATIC_GROUP_NEW size");

struct LAND_TILES
{
    uint64_t Flags;
    unsigned short TexID;
    string Name; // FIXME
};
static_assert(sizeof(LAND_TILES) == 42, "Invalid LAND_TILES size");
/*
struct LAND_GROUP
{
    unsigned int Unknown;
    LAND_TILES Tiles[32];
};
static_assert(sizeof(LAND_GROUP) == 0, "Invalid LAND_GROUP size");
*/
struct STATIC_TILES
{
    uint64_t Flags;
    unsigned char Weight;
    unsigned char Layer;
    unsigned int Count;
    unsigned short AnimID;
    unsigned short Hue;
    unsigned short LightIndex;
    unsigned char Height;
    string Name; // FIXME
};
static_assert(sizeof(STATIC_TILES) == 53, "Invalid STATIC_TILES size");
/*
struct STATIC_GROUP
{
    unsigned int Unk;
    STATIC_TILES Tiles[32];
};
static_assert(sizeof(STATIC_GROUP) == 0, "Invalid STATIC_GROUP size");
*/
struct MULTI_IDX_BLOCK : public BASE_IDX_BLOCK // FIXME
{
    unsigned int Unknown;
};
static_assert(sizeof(MULTI_IDX_BLOCK) == 12, "Invalid MULTI_IDX_BLOCK size");

struct MULTI_BLOCK
{
    unsigned short ID;
    short X;
    short Y;
    short Z;
    unsigned int Flags;
};
static_assert(sizeof(MULTI_BLOCK) == 12, "Invalid MULTI_BLOCK size");

struct MULTI_BLOCK_NEW
{
    unsigned short ID;
    short X;
    short Y;
    short Z;
    unsigned int Flags;
    int Unknown;
};
static_assert(sizeof(MULTI_BLOCK_NEW) == 16, "Invalid MULTI_BLOCK_NEW size");

struct HUES_BLOCK
{
    unsigned short ColorTable[32];
    unsigned short TableStart;
    unsigned short TableEnd;
    char Name[20];
};
static_assert(sizeof(RADAR_MAP_CELLS) == 4, "Invalid RADAR_MAP_CELLS size");

struct HUES_GROUP
{
    unsigned int Header;
    HUES_BLOCK Entries[8];
};
static_assert(sizeof(HUES_BLOCK) == 88, "Invalid HUES_BLOCK size");

struct VERDATA_HUES_BLOCK
{
    unsigned short ColorTable[32];
    unsigned short TableStart;
    unsigned short TableEnd;
    char Name[20];
    unsigned short Unk[32];
};
static_assert(sizeof(VERDATA_HUES_BLOCK) == 152, "Invalid VERDATA_HUES_BLOCK size");

struct VERDATA_HUES_GROUP
{
    unsigned int Header;
    VERDATA_HUES_BLOCK Entries[8];
};
static_assert(sizeof(VERDATA_HUES_GROUP) == 1220, "Invalid VERDATA_HUES_GROUP size");

struct GUMP_IDX_BLOCK : public BASE_IDX_BLOCK // FIXME
{
    unsigned short Height;
    unsigned short Width;
};
static_assert(sizeof(GUMP_IDX_BLOCK) == 12, "Invalid GUMP_IDX_BLOCK size");

struct GUMP_BLOCK
{
    unsigned short Value;
    unsigned short Run;
};
static_assert(sizeof(GUMP_BLOCK) == 4, "Invalid GUMP_BLOCK size");

struct SKILLS_IDX_BLOCK : public BASE_IDX_BLOCK // FIXME
{
    unsigned int Unknown;
};
static_assert(sizeof(SKILLS_IDX_BLOCK) == 12, "Invalid SKILLS_IDX_BLOCK size");
/*
struct SKILLS_BLOCK
{
    unsigned char Button;
    char *SkillName;
};
static_assert(sizeof(SKILLS_BLOCK) == 0, "Invalid SKILLS_BLOCK size");
*/
/*
struct BODYCONV_DATA
{
    short Anim2;
    short Anim3;
    short Anim4;
    short Anim5;

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
    unsigned char Unknown;
    unsigned char FrameCount;
    unsigned char FrameInterval;
    unsigned char FrameStart;
};
static_assert(sizeof(ANIM_DATA) == 68, "Invalid ANIM_DATA size");

struct ART_IDX_BLOCK : public BASE_IDX_BLOCK // FIXME
{
    unsigned int Unknown;
};
static_assert(sizeof(ART_IDX_BLOCK) == 12, "Invalid ART_IDX_BLOCK size");

struct LIGHT_IDX_BLOCK : public BASE_IDX_BLOCK // FIXME
{
    unsigned short Height;
    unsigned short Width;
};
static_assert(sizeof(LIGHT_IDX_BLOCK) == 12, "Invalid LIGHT_IDX_BLOCK size");

struct ANIM_IDX_BLOCK : public BASE_IDX_BLOCK // FIXME
{
    unsigned int Unknown;
};
static_assert(sizeof(ANIM_IDX_BLOCK) == 12, "Invalid ANIM_IDX_BLOCK size");

struct TEXTURE_IDX_BLOCK : public BASE_IDX_BLOCK // FIXME
{
    unsigned int Unknown;
};
static_assert(sizeof(TEXTURE_IDX_BLOCK) == 12, "Invalid TEXTURE_IDX_BLOCK size");
/*
struct PALETTE_BLOCK
{
    unsigned char R;
    unsigned char G;
    unsigned char B;
};
static_assert(sizeof(PALETTE_BLOCK) == 0, "Invalid PALETTE_BLOCK size");
*/
#ifndef XUOASSISTANT_LIBRARY

struct FONT_HEADER
{
    unsigned char Width;
    unsigned char Height;
    unsigned char Unknown;
};
static_assert(sizeof(FONT_HEADER) == 3, "Invalid FONT_HEADER size");

struct FONT_CHARACTER
{
    unsigned char Width;
    unsigned char Height;
    unsigned char Unknown;
};
static_assert(sizeof(FONT_CHARACTER) == 3, "Invalid FONT_CHARACTER size");

struct FONT_CHARACTER_DATA
{
    unsigned char Width;
    unsigned char Height;
    vector<uint16_t> Data;
};
//static_assert(sizeof(FONT_CHARACTER_DATA) == 0, "Invalid FONT_CHARACTER_DATA size");

struct FONT_DATA
{
    unsigned char Header;
    FONT_CHARACTER_DATA Chars[224];
};
//static_assert(sizeof(FONT_DATA) == 0, "Invalid FONT_DATA size");

struct FONT_OBJECT
{
    unsigned char Width;
    unsigned char Height;
    GLuint Texture;
    unsigned char *Data;
};
static_assert(sizeof(FONT_OBJECT) == 14, "Invalid FONT_OBJECT size");

struct UNICODE_FONT_DATA
{
    unsigned char OffsetX;
    unsigned char OffsetY;
    unsigned char Width;
    unsigned char Height;
};
static_assert(sizeof(UNICODE_FONT_DATA) == 4, "Invalid UNICODE_FONT_DATA size");

struct SOUND_IDX_BLOCK : public BASE_IDX_BLOCK // FIXME
{
    unsigned short Index;
    unsigned short Reserved;
};
static_assert(sizeof(SOUND_IDX_BLOCK) == 12, "Invalid SOUND_IDX_BLOCK size");

struct SOUND_BLOCK
{
    char Name[16];
    unsigned int Unknown1;
    unsigned int Unknown2;
    unsigned int Unknown3;
    unsigned int Unknown4;
    //Data;
};
static_assert(sizeof(SOUND_BLOCK) == 32, "Invalid SOUND_BLOCK size");

#endif // XUOASSISTANT_LIBRARY

#pragma pack(pop)
