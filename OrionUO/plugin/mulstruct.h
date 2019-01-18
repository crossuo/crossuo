// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <stdint.h>
#include <string>

#pragma pack(push, 1)

struct VERDATA_HEADER
{
    unsigned int FileID;
    unsigned int BlockID;
    unsigned int Position;
    unsigned int Size;
    unsigned int GumpData;
};

struct BASE_IDX_BLOCK
{
    unsigned int Position;
    unsigned int Size;
};

struct MAP_CELLS_EX
{
    unsigned short TileID;
    unsigned short Color;
    char Z;
};

struct MAP_CELLS
{
    unsigned short TileID;
    char Z;
};

struct MAP_BLOCK
{
    unsigned int Header;
    MAP_CELLS Cells[64];
};

struct RADAR_MAP_CELLS
{
    unsigned short Graphic;
    char Z;
    char IsLand;
};
struct RADAR_MAP_BLOCK
{
    RADAR_MAP_CELLS Cells[8][8];
};

struct STAIDX_BLOCK : public BASE_IDX_BLOCK
{
    unsigned int Unknown;
};

struct STATICS_BLOCK
{
    unsigned short Color;
    unsigned char X;
    unsigned char Y;
    char Z;
    unsigned short Hue;
};

struct LAND_TILES_OLD
{
    unsigned int Flags;
    unsigned short TexID;
    char Name[20];
};

struct LAND_GROUP_OLD
{
    unsigned int Unknown;
    LAND_TILES_OLD Tiles[32];
};

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

struct STATIC_GROUP_OLD
{
    unsigned int Unk;
    STATIC_TILES_OLD Tiles[32];
};

struct LAND_TILES_NEW
{
    uint64_t Flags;
    unsigned short TexID;
    char Name[20];
};

struct LAND_GROUP_NEW
{
    unsigned int Unknown;
    LAND_TILES_NEW Tiles[32];
};

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

struct STATIC_GROUP_NEW
{
    unsigned int Unk;
    STATIC_TILES_NEW Tiles[32];
};

struct LAND_TILES
{
    uint64_t Flags;
    unsigned short TexID;
    string Name;
};

struct LAND_GROUP
{
    unsigned int Unknown;
    LAND_TILES Tiles[32];
};

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
    string Name;
};

struct STATIC_GROUP
{
    unsigned int Unk;
    STATIC_TILES Tiles[32];
};

struct MULTI_IDX_BLOCK : public BASE_IDX_BLOCK
{
    unsigned int Unknown;
};

struct MULTI_BLOCK
{
    unsigned short ID;
    short X;
    short Y;
    short Z;
    unsigned int Flags;
};

struct MULTI_BLOCK_NEW
{
    unsigned short ID;
    short X;
    short Y;
    short Z;
    unsigned int Flags;
    int Unknown;
};

struct HUES_BLOCK
{
    unsigned short ColorTable[32];
    unsigned short TableStart;
    unsigned short TableEnd;
    char Name[20];
};

struct HUES_GROUP
{
    unsigned int Header;
    HUES_BLOCK Entries[8];
};

struct VERDATA_HUES_BLOCK
{
    unsigned short ColorTable[32];
    unsigned short TableStart;
    unsigned short TableEnd;
    char Name[20];
    unsigned short Unk[32];
};

struct VERDATA_HUES_GROUP
{
    unsigned int Header;
    VERDATA_HUES_BLOCK Entries[8];
};

struct GUMP_IDX_BLOCK : public BASE_IDX_BLOCK
{
    unsigned short Height;
    unsigned short Width;
};

struct GUMP_BLOCK
{
    unsigned short Value;
    unsigned short Run;
};

struct SKILLS_IDX_BLOCK : public BASE_IDX_BLOCK
{
    unsigned int Unknown;
};

struct SKILLS_BLOCK
{
    unsigned char Button;
    char *SkillName;
};

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

struct ANIM_DATA
{
    char FrameData[64];
    unsigned char Unknown;
    unsigned char FrameCount;
    unsigned char FrameInterval;
    unsigned char FrameStart;
};

struct ART_IDX_BLOCK : public BASE_IDX_BLOCK
{
    unsigned int Unknown;
};

struct LIGHT_IDX_BLOCK : public BASE_IDX_BLOCK
{
    unsigned short Height;
    unsigned short Width;
};

struct ANIM_IDX_BLOCK : public BASE_IDX_BLOCK
{
    unsigned int Unknown;
};

struct TEXTURE_IDX_BLOCK : public BASE_IDX_BLOCK
{
    unsigned int Unknown;
};

struct PALETTE_BLOCK
{
    unsigned char R;
    unsigned char G;
    unsigned char B;
};

#ifndef ORIONASSISTANT_LIBRARY

struct FONT_HEADER
{
    unsigned char Width;
    unsigned char Height;
    unsigned char Unknown;
};

struct FONT_CHARACTER
{
    unsigned char Width;
    unsigned char Height;
    unsigned char Unknown;
};

struct FONT_CHARACTER_DATA
{
    unsigned char Width;
    unsigned char Height;
    vector<uint16_t> Data;
};

struct FONT_DATA
{
    unsigned char Header;
    FONT_CHARACTER_DATA Chars[224];
};

struct FONT_OBJECT
{
    unsigned char Width;
    unsigned char Height;
    GLuint Texture;
    unsigned char *Data;
};

struct UNICODE_FONT_DATA
{
    unsigned char OffsetX;
    unsigned char OffsetY;
    unsigned char Width;
    unsigned char Height;
};

struct SOUND_IDX_BLOCK : public BASE_IDX_BLOCK
{
    unsigned short Index;
    unsigned short Reserved;
};

struct SOUND_BLOCK
{
    char Name[16];
    unsigned int Unknown1;
    unsigned int Unknown2;
    unsigned int Unknown3;
    unsigned int Unknown4;
    //Data;
};

#endif // ORIONASSISTANT_LIBRARY

#pragma pack(pop)
