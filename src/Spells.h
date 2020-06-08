// AGPLv3 License
// Copyright (c) 2020 Danny Angelo Carminati Grein
#pragma once

#include <xuocore/enumlist.h>

const int MAX_SPELLS_COUNT = 64;

enum class SpellCount : uint16_t
{
    Magery = 64,
    Necromancy = 17u,
    Chivalry = 10,
    Bushido = 6,
    Ninjitsu = 8,
    Spellweaving = 16,
    Mysticism = 16,
    Mastery = 45,
};

enum class SpellOffset : size_t
{
    Magery = 0,
    Necromancy = Magery + int(SpellCount::Magery),
    Chivalry = Necromancy + int(SpellCount::Necromancy),
    Bushido = Chivalry + int(SpellCount::Chivalry),
    Ninjitsu = Bushido + int(SpellCount::Bushido),
    Spellweaving = Ninjitsu + int(SpellCount::Ninjitsu),
    Mysticism = Spellweaving + int(SpellCount::Spellweaving),
    Mastery = Mysticism + int(SpellCount::Mysticism),
};

enum TARGET_TYPE : uint8_t
{
    TT_NEUTRAL,
    TT_BENEFICIAL,
    TT_HARMFUL,
};

struct Spell
{
    SPELLBOOK_TYPE Type = ST_INVALID;
    uint8_t IndexOffset = 0;
    uint16_t Id = 0;
    const char *Name = nullptr;
    const char *PowerWords = nullptr;
    uint16_t BookIconGraphic = 0;
    uint16_t SpellIconGraphic = 0;
    uint32_t DescriptionCliloc = 0;
    uint32_t NameCliloc = 0;
    uint16_t MinSkill = 0;
    uint16_t ManaCost = 0;
    uint16_t TithingCost = 0;
    TARGET_TYPE TargetType = TT_NEUTRAL;
    const char *Reagents = 0;
};

struct SpellBook
{
    SPELLBOOK_TYPE Type = ST_INVALID;
    uint16_t Graphic = 0;
    uint16_t MinimizedGraphic = 0;
    uint16_t SpellsPerPage = 0;
    uint16_t IndexPagesCount = 0;
    uint16_t SpellCount = 0;
    bool HasSeparator = false;
    const Spell *Spells[MAX_SPELLS_COUNT];
};

void InitSpells();
const Spell &GetSpell(int offset);
const SpellBook &GetSpellbook(SPELLBOOK_TYPE type);
const SpellBook &GetSpellbook(int type);
const Spell *GetSpellByOffsetAndType(int offset, SPELLBOOK_TYPE spellType);
const Spell *GetSpellByGraphicAndType(uint16_t graphic, SPELLBOOK_TYPE spellType);