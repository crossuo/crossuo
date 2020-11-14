
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include <vector>
#include <algorithm> // std::min
#include <cmath>     // ceilf
#include <assert.h>
#include "Spells.h"
#include "Config.h"

static std::vector<Spell> s_Spells;
static std::vector<SpellBook> s_SpellBooks;

const Spell &GetSpell(int offset)
{
    return s_Spells[offset];
}

const SpellBook &GetSpellbook(SPELLBOOK_TYPE type)
{
    assert(type >= ST_FIRST && type < ST_COUNT);
    return s_SpellBooks[type];
}

const SpellBook &GetSpellbook(int type)
{
    assert(type >= ST_FIRST && type < ST_COUNT);
    return s_SpellBooks[type];
}

const Spell *GetSpellByOffsetAndType(int offset, SPELLBOOK_TYPE spellType)
{
    const auto book = GetSpellbook(spellType);
    assert(offset < book.SpellCount);
    const auto spell = book.Spells[offset];
    return spell;
}

const Spell *GetSpellByGraphicAndType(uint16_t graphic, SPELLBOOK_TYPE spellType)
{
    const auto book = GetSpellbook(spellType);
    for (int s = 0; s < book.SpellCount; s++)
    {
        const auto spell = book.Spells[s];
        if (spell->SpellIconGraphic == graphic)
            return spell;
    }
    assert(false && "invalid spell");
    return nullptr;
}

void InitSpells()
{
#define VLE(ver, a, b) uint16_t(g_Config.ClientVersion <= ver ? a : b)
    // clang-format off
    // Magery
    s_Spells.push_back({ ST_MAGERY, 0, 1, "Clumsy", "U J", 0x08c0, 0x08c0, 1061290, 3002011, 0, 0, 0, TT_HARMFUL, "Bloodmoss\nNightshade" });
    s_Spells.push_back({ ST_MAGERY, 1, 2, "Create Food", "I M Y", 0x08c1, 0x08c1, 1061291, 3002012, 0, 0, 0, TT_NEUTRAL, "Garlic\nGinseng\nMandrake Root" });
    s_Spells.push_back({ ST_MAGERY, 2, 3, "Feeblemind", "R W", 0x08c2, 0x08c2, 1061292, 3002013, 0, 0, 0, TT_HARMFUL, "Nightshade\nGinseng" });
    s_Spells.push_back({ ST_MAGERY, 3, 4, "Heal", "I M", 0x08c3, 0x08c3, 1061293, 3002014, 0, 0, 0, TT_BENEFICIAL, "Garlic\nGinseng\nSpiders' Silk" });
    s_Spells.push_back({ ST_MAGERY, 4, 5, "Magic Arrow", "I P Y", 0x08c4, 0x08c4, 1061294, 3002015, 0, 0, 0, TT_HARMFUL, "Sulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 5, 6, "Night Sight", "I L", 0x08c5, 0x08c5, 1061295, 3002016, 0, 0, 0, TT_BENEFICIAL, "Spiders' Silk\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 6, 7, "Reactive Armor", "F S", 0x08c6, 0x08c6, 1061296, 3002017, 0, 0, 0, TT_BENEFICIAL, "Garlic\nSpiders' Silk\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 7, 8, "Weaken", "D M", 0x08c7, 0x08c7, 1061297, 3002018, 0, 0, 0, TT_HARMFUL, "Garlic\nNightshade" });
    s_Spells.push_back({ ST_MAGERY, 8, 9, "Agility", "E U", 0x08c8, 0x08c8, 1061298, 3002019, 0, 0, 0, TT_BENEFICIAL, "Bloodmoss\nMandrake Root" });
    s_Spells.push_back({ ST_MAGERY, 9, 10, "Cunning", "U W", 0x08c9, 0x08c9, 1061299, 3002020, 0, 0, 0, TT_BENEFICIAL, "Nightshade\nMandrake Root" });
    s_Spells.push_back({ ST_MAGERY, 10, 11, "Cure", "A N", 0x08ca, 0x08ca, 1061300, 3002021, 0, 0, 0, TT_BENEFICIAL, "Garlic\nGinseng" });
    s_Spells.push_back({ ST_MAGERY, 11, 12, "Harm", "A M", 0x08cb, 0x08cb, 1061301, 3002022, 0, 0, 0, TT_HARMFUL, "Nightshade\nSpiders' Silk" });
    s_Spells.push_back({ ST_MAGERY, 12, 13, "Magic Trap", "I J", 0x08cc, 0x08cc, 1061302, 3002023, 0, 0, 0, TT_NEUTRAL, "Garlic\nSpiders' Silk\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 13, 14, "Magic Untrap", "A J", 0x08cd, 0x08cd, 1061303, 3002024, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 14, 15, "Protection", "U S", 0x08ce, 0x08ce, 1061304, 3002025, 0, 0, 0, TT_BENEFICIAL, "Garlic\nGinseng\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 15, 16, "Strength", "U M", 0x08cf, 0x08cf, 1061305, 3002026, 0, 0, 0, TT_BENEFICIAL, "Mandrake Root\nNightshade" });
    s_Spells.push_back({ ST_MAGERY, 16, 17, "Bless", "R S", 0x08d0, 0x08d0, 1061306, 3002027, 0, 0, 0, TT_BENEFICIAL, "Garlic\nMandrake Root" });
    s_Spells.push_back({ ST_MAGERY, 17, 18, "Fireball", "V F", 0x08d1, 0x08d1, 1061307, 3002028, 0, 0, 0, TT_HARMFUL, "Black Pearl" });
    s_Spells.push_back({ ST_MAGERY, 18, 19, "Magic Lock", "A P", 0x08d2, 0x08d2, 1061308, 3002029, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nGarlic\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 19, 20, "Poison", "I N", 0x08d3, 0x08d3, 1061309, 3002030, 0, 0, 0, TT_HARMFUL, "Nightshade" });
    s_Spells.push_back({ ST_MAGERY, 20, 21, "Telekinesis", "O P Y", 0x08d4, 0x08d4, 1061310, 3002031, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nMandrake Root" });
    s_Spells.push_back({ ST_MAGERY, 21, 22, "Teleport", "R P", 0x08d5, 0x08d5, 1061311, 3002032, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nMandrake Root" });
    s_Spells.push_back({ ST_MAGERY, 22, 23, "Unlock", "E P", 0x08d6, 0x08d6, 1061312, 3002033, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 23, 24, "Wall of Stone", "I S Y", 0x08d7, 0x08d7, 1061313, 3002034, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nGarlic" });
    s_Spells.push_back({ ST_MAGERY, 24, 25, "Arch Cure", "V A N", 0x08d8, 0x08d8, 1061314, 3002035, 0, 0, 0, TT_BENEFICIAL, "Garlic\nGinseng\nMandrake Root" });
    s_Spells.push_back({ ST_MAGERY, 25, 26, "Arch Protection", "V U S", 0x08d9, 0x08d9, 1061315, 3002036, 0, 0, 0, TT_BENEFICIAL, "Garlic\nGinseng\nMandrake Root\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 26, 27, "Curse", "D S", 0x08da, 0x08da, 1061316, 3002037, 0, 0, 0, TT_HARMFUL, "Garlic\nNightshade\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 27, 28, "Fire Field", "I F G", 0x08db, 0x08db, 1061317, 3002038, 0, 0, 0, TT_NEUTRAL, "Black Pearl\nSpiders' Silk\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 28, 29, "Greater Heal", "I V M", 0x08dc, 0x08dc, 1061318, 3002039, 0, 0, 0, TT_BENEFICIAL, "Garlic\nGinseng\nMandrake Root\nSpiders' Silk" });
    s_Spells.push_back({ ST_MAGERY, 29, 30, "Lightning", "P O G", 0x08dd, 0x08dd, 1061319, 3002040, 0, 0, 0, TT_HARMFUL, "Mandrake Root\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 30, 31, "Mana Drain", "O R", 0x08de, 0x08de, 1061320, 3002041, 0, 0, 0, TT_HARMFUL, "Black Pearl\nMandrake Root\nSpiders' Silk" });
    s_Spells.push_back({ ST_MAGERY, 31, 32, "Recall", "K O P", 0x08df, 0x08df, 1061321, 3002042, 0, 0, 0, TT_NEUTRAL, "Black Pearl\nBloodmoss\nMandrake Root" });
    s_Spells.push_back({ ST_MAGERY, 32, 33, "Blade Spirits", "I J H Y", 0x08e0, 0x08e0, 1061322, 3002043, 0, 0, 0, TT_NEUTRAL, "Black Pearl\nMandrake Root\nNightshade" });
    s_Spells.push_back({ ST_MAGERY, 33, 34, "Dispel Field", "A G", 0x08e1, 0x08e1, 1061323, 3002044, 0, 0, 0, TT_NEUTRAL, "Black Pearl\nGarlic\nSpiders' Silk\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 34, 35, "Incognito", "K I E", 0x08e2, 0x08e2, 1061324, 3002045, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nGarlic\nNightshade" });
    s_Spells.push_back({ ST_MAGERY, 35, 36, "Magic Reflection", "I J S", 0x08e3, 0x08e3, 1061325, 3002046, 0, 0, 0, TT_BENEFICIAL, "Garlic\nMandrake Root\nSpiders' Silk" });
    s_Spells.push_back({ ST_MAGERY, 36, 37, "Mind Blast", "P C W", 0x08e4, 0x08e4, 1061326, 3002047, 0, 0, 0, TT_HARMFUL, "Black Pearl\nMandrake Root\nNightshade\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 37, 38, "Paralyze", "A E P", 0x08e5, 0x08e5, 1061327, 3002048, 0, 0, 0, TT_HARMFUL, "Garlic\nMandrake Root\nSpiders' Silk" });
    s_Spells.push_back({ ST_MAGERY, 38, 39, "Poison Field", "I N G", 0x08e6, 0x08e6, 1061328, 3002049, 0, 0, 0, TT_NEUTRAL, "Black Pearl\nNightshade\nSpiders' Silk" });
    s_Spells.push_back({ ST_MAGERY, 39, 40, "Summon Creature", "K X", 0x08e7, 0x08e7, 1061329, 3002050, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nMandrake Root\nSpiders' Silk" });
    s_Spells.push_back({ ST_MAGERY, 40, 41, "Dispel", "A O", 0x08e8, 0x08e8, 1061330, 3002051, 0, 0, 0, TT_NEUTRAL, "Garlic\nMandrake Root\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 41, 42, "Energy Bolt", "C P", 0x08e9, 0x08e9, 1061331, 3002052, 0, 0, 0, TT_HARMFUL, "Black Pearl\nNightshade" });
    s_Spells.push_back({ ST_MAGERY, 42, 43, "Explosion", "V O F", 0x08ea, 0x08ea, 1061332, 3002053, 0, 0, 0, TT_HARMFUL, "Bloodmoss\nMandrake Root" });
    s_Spells.push_back({ ST_MAGERY, 43, 44, "Invisibility", "A L X", 0x08eb, 0x08eb, 1061333, 3002054, 0, 0, 0, TT_BENEFICIAL, "Bloodmoss\nNightshade" });
    s_Spells.push_back({ ST_MAGERY, 44, 45, "Mark", "K P Y", 0x08ec, 0x08ec, 1061334, 3002055, 0, 0, 0, TT_NEUTRAL, "Black Pearl\nBloodmoss\nMandrake Root" });
    s_Spells.push_back({ ST_MAGERY, 45, 46, "Mass Curse", "V D S", 0x08ed, 0x08ed, 1061335, 3002056, 0, 0, 0, TT_HARMFUL, "Garlic\nMandrake Root\nNightshade\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 46, 47, "Paralyze Field", "I E G", 0x08ee, 0x08ee, 1061336, 3002057, 0, 0, 0, TT_NEUTRAL, "Black Pearl\nGinseng\nSpiders' Silk" });
    s_Spells.push_back({ ST_MAGERY, 47, 48, "Reveal", "W Q", 0x08ef, 0x08ef, 1061337, 3002058, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 48, 49, "Chain Lightning", "V O G", 0x08f0, 0x08f0, 1061338, 3002059, 0, 0, 0, TT_HARMFUL, "Black Pearl\nBloodmoss\nMandrake Root\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 49, 50, "Energy Field", "I S G", 0x08f1, 0x08f1, 1061339, 3002060, 0, 0, 0, TT_NEUTRAL, "Black Pearl\nMandrake Root\nSpiders' Silk\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 50, 51, "Flamestrike", "K V F", 0x08f2, 0x08f2, 1061340, 3002061, 0, 0, 0, TT_HARMFUL, "Spiders' Silk\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 51, 52, "Gate Travel", "V R P", 0x08f3, 0x08f3, 1061341, 3002062, 0, 0, 0, TT_NEUTRAL, "Black Pearl\nMandrake Root\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 52, 53, "Mana Vampire", "O S", 0x08f4, 0x08f4, 1061342, 3002063, 0, 0, 0, TT_HARMFUL, "Black Pearl\nBloodmoss\nMandrake Root\nSpiders' Silk" });
    s_Spells.push_back({ ST_MAGERY, 53, 54, "Mass Dispel", "V A O", 0x08f5, 0x08f5, 1061343, 3002064, 0, 0, 0, TT_NEUTRAL, "Black Pearl\nGarlic\nMandrake Root\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 54, 55, "Meteor Swarm", "F K D Y", 0x08f6, 0x08f6, 1061344, 3002065, 0, 0, 0, TT_HARMFUL, "Bloodmoss\nMandrake Root\nSpiders' Silk\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 55, 56, "Polymorph", "V Y R", 0x08f7, 0x08f7, 1061345, 3002066, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nMandrake Root\nSpiders' Silk" });
    s_Spells.push_back({ ST_MAGERY, 56, 57, "Earthquake", "I V P", 0x08f8, 0x08f8, 1061346, 3002067, 0, 0, 0, TT_HARMFUL, "Bloodmoss\nGinseng\nMandrake Root\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 57, 58, "Energy Vortex", "V C P", 0x08f9, 0x08f9, 1061347, 3002068, 0, 0, 0, TT_NEUTRAL, "Black Pearl\nBloodmoss\nMandrake Root\nNightshade" });
    s_Spells.push_back({ ST_MAGERY, 58, 59, "Resurrection", "A C", 0x08fa, 0x08fa, 1061348, 3002069, 0, 0, 0, TT_BENEFICIAL, "Bloodmoss\nGinseng\nGarlic" });
    s_Spells.push_back({ ST_MAGERY, 59, 60, "Air Elemental", "K V X H", 0x08fb, 0x08fb, 1061349, 3002070, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nMandrake Root\nSpiders' Silk" });
    s_Spells.push_back({ ST_MAGERY, 60, 61, "Summon Daemon", "K V X C", 0x08fc, 0x08fc, 1061350, 3002071, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nMandrake Root\nSpiders' Silk\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 61, 62, "Earth Elemental", "K V X Y", 0x08fd, 0x08fd, 1061351, 3002072, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nMandrake Root\nSpiders' Silk" });
    s_Spells.push_back({ ST_MAGERY, 62, 63, "Fire Elemental", "K V X F", 0x08fe, 0x08fe, 1061352, 3002073, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nMandrake Root\nSpiders' Silk\nSulfurous Ash" });
    s_Spells.push_back({ ST_MAGERY, 63, 64, "Water Elemental", "K V X A F", 0x08ff, 0x08ff, 1061353, 0, 0, 0, 0, TT_NEUTRAL, "Bloodmoss\nMandrake Root\nSpiders' Silk" });
    // Necromancy
    assert(s_Spells.size() == int(SpellOffset::Necromancy));
    s_Spells.push_back({ ST_NECROMANCY, 0, 101, "Animate Dead", "Uus Corp", 0x5000, 0x5000, 1061390, 1060509, 40, 23, 0, TT_NEUTRAL, "Daemon Blood\nGrave Dust" });
    s_Spells.push_back({ ST_NECROMANCY, 1, 102, "Blood Oath", "In Jux Mani Xen", 0x5001, 0x5001, 1061391, 1060510, 20, 13, 0, TT_HARMFUL, "Daemon Blood" });
    s_Spells.push_back({ ST_NECROMANCY, 2, 103, "Corpse Skin", "In Agle Corp Ylem", 0x5002, 0x5002, 1061392, 1060511, 20, 11, 0, TT_HARMFUL, "Bat Wing\nGrave Dust" });
    s_Spells.push_back({ ST_NECROMANCY, 3, 104, "Curse Weapon", "An Sanct Gra Char", 0x5003, 0x5003, 1061393, 1060512, 0, 7, 0, TT_NEUTRAL, "Pig Iron" });
    s_Spells.push_back({ ST_NECROMANCY, 4, 105, "Evil Omen", "Pas Tym An Sanct", 0x5004, 0x5004, 1061394, 1060513, 20, 11, 0, TT_HARMFUL, "Bat Wing\nNox Crystal" });
    s_Spells.push_back({ ST_NECROMANCY, 5, 106, "Horrific Beast", "Rel Xen Vas Bal", 0x5005, 0x5005, 1061395, 1060514, 40, 11, 0, TT_NEUTRAL, "Bat Wing\nDaemon Blood" });
    s_Spells.push_back({ ST_NECROMANCY, 6, 107, "Lich Form", "Rel Xen Corp Ort", 0x5006, 0x5006, 1061396, 1060515, 70, VLE(CV_7000, 25, 23), 0, TT_NEUTRAL, "Daemon Blood\nGrave Dust\nNox Crystal" });
    s_Spells.push_back({ ST_NECROMANCY, 7, 108, "Mind Rot", "Wis An Ben", 0x5007, 0x5007, 1061397, 1060516, 30, 17, 0, TT_HARMFUL, "Bat Wing\nDaemon Blood\nPig Iron" });
    s_Spells.push_back({ ST_NECROMANCY, 8, 109, "Pain Spike", "In Sar", 0x5008, 0x5008, 1061398, 1060517, 20, 5, 0, TT_HARMFUL, "Grave Dust\nPig Iron" });
    s_Spells.push_back({ ST_NECROMANCY, 9, 110, "Poison Strike", "In Vas Nox", 0x5009, 0x5009, 1061399, 1060518, 50, 17, 0, TT_HARMFUL, "Nox Crystal" });
    s_Spells.push_back({ ST_NECROMANCY, 10, 111, "Strangle", "In Bal Nox", 0x500a, 0x500a, 1061400, 1060519, 65, 29, 0, TT_HARMFUL, "Daemon Blood\nNox Crystal" });
    s_Spells.push_back({ ST_NECROMANCY, 11, 112, "Summon Familiar", "Kal Xen Bal", 0x500b, 0x500b, 1061401, 1060520, 30, 17, 0, TT_NEUTRAL, "Bat Wing\nDaemon Blood\nGrave Dust" });
    s_Spells.push_back({ ST_NECROMANCY, 12, 113, "Vampiric Embrace", "Rel Xen An Sanct", 0x500c, 0x500c, 1061402, 1060521, 99, VLE(CV_7000, 25, 23), 0, TT_NEUTRAL, "Bat Wing\nNox Crystal\nPig Iron" });
    s_Spells.push_back({ ST_NECROMANCY, 13, 114, "Vengeful Spirit", "Kal Xen Bal Beh", 0x500d, 0x500d, 1061403, 1060522, 80, 41, 0, TT_HARMFUL, "Bat Wing\nGrave Dust\nPig Iron" });
    s_Spells.push_back({ ST_NECROMANCY, 14, 115, "Wither", "Kal Vas An Flam", 0x500e, 0x500e, 1061404, 1060523, 60, 23, 0, TT_HARMFUL, "Grave Dust\nNox Crystal\nPig Iron" });
    s_Spells.push_back({ ST_NECROMANCY, 15, 116, "Wraith Form", "Rel Xen Um", 0x500f, 0x500f, 1061405, 1060524, 20, 17, 0, TT_NEUTRAL, "Nox Crystal\nPig Iron" });
    s_Spells.push_back({ ST_NECROMANCY, 16, 117, "Exorcism", "Ort Corp Grav", 0x5010, 0x5010, 1061406, 1060525, 80, 40, 0, TT_NEUTRAL, "Nox Crystal\nGrave Dust" });
    // Chivalry
    assert(s_Spells.size() == int(SpellOffset::Chivalry));
    s_Spells.push_back({ ST_CHIVALRY, 0, 201, "Cleanse by Fire", "Expor Flamus", 0x5100, 0x5100, 1061490, 1060585, 5, 10, 10, TT_BENEFICIAL, nullptr });
    s_Spells.push_back({ ST_CHIVALRY, 1, 202, "Close Wounds", "Obsu Vulni", 0x5101, 0x5101, 1061491, 1060586, 0, 10, 10, TT_BENEFICIAL, nullptr });
    s_Spells.push_back({ ST_CHIVALRY, 2, 203, "Consecrate Weapon", "Consecrus Arma", 0x5102, 0x5102, 1061492, 1060587, 15, 10, 10, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_CHIVALRY, 3, 204, "Dispel Evil", "Dispiro Malas", 0x5103, 0x5103, 1061493, 1060588, 35, 10, 10, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_CHIVALRY, 4, 205, "Divine Fury", "Divinum Furis", 0x5104, 0x5104, 1061494, 1060589, 25, 10, 10, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_CHIVALRY, 5, 206, "Enemy of One", "Forul Solum", 0x5105, 0x5105, 1061495, 1060590, 45, 20, 10, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_CHIVALRY, 6, 207, "Holy Light", "Augus Luminos", 0x5106, 0x5106, 1061496, 1060591, 55, 20, 10, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_CHIVALRY, 7, 208, "Noble Sacrifice", "Dium Prostra", 0x5107, 0x5107, 1061497, 1060592, 65, 20, 30, TT_BENEFICIAL, nullptr });
    s_Spells.push_back({ ST_CHIVALRY, 8, 209, "Remove Curse", "Extermo Vomica", 0x5108, 0x5108, 1061498, 1060593, 5, 20, 10, TT_BENEFICIAL, nullptr });
    s_Spells.push_back({ ST_CHIVALRY, 9, 210, "Sacred Journey", "Sanctum Viatas", 0x5109, 0x5109, 1061499, 1060594, 5, 20, 10, TT_NEUTRAL, nullptr });
    // Bushido
    assert(s_Spells.size() == int(SpellOffset::Bushido));
    s_Spells.push_back({ ST_BUSHIDO, 0, 401, "Honorable Execution", nullptr, 0x5400, 0x5420, 1063263, 1060595, 25, 0, 0, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_BUSHIDO, 1, 402, "Confidence", nullptr, 0x5401, 0x5421, 1063264, 1060596, 25, 10, 0, TT_BENEFICIAL, nullptr });
    s_Spells.push_back({ ST_BUSHIDO, 2, 403, "Evasion", nullptr, 0x5402, 0x5422, 1063265, 1060597, 60, 10, 0, TT_BENEFICIAL, nullptr });
    s_Spells.push_back({ ST_BUSHIDO, 3, 404, "Counter Attack", nullptr, 0x5403, 0x5423, 1063266, 1060598, 40, 5, 0, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_BUSHIDO, 4, 405, "Lightning Strike", nullptr, 0x5404, 0x5424, 1063267, 1060599, 50, 10, 0, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_BUSHIDO, 5, 406, "Momentum Strike", nullptr, 0x5405, 0x5425, 1063268, 1060600, 70, 10, 0, TT_HARMFUL, nullptr });
    // Ninjitsu
    assert(s_Spells.size() == int(SpellOffset::Ninjitsu));
    s_Spells.push_back({ ST_NINJITSU, 0, 501, "Focus Attack", nullptr, 0x5300, 0x5320, 1063279, 1060610, 60, 20, 0, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_NINJITSU, 1, 502, "Death Strike", nullptr, 0x5301, 0x5321, 1063280, 1060611, 85, 30, 0, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_NINJITSU, 2, 503, "Animal Form", nullptr, 0x5302, 0x5322, 1063281, 1060612, 10, 0, 0, TT_BENEFICIAL, nullptr });
    s_Spells.push_back({ ST_NINJITSU, 3, 504, "Ki Attack", nullptr, 0x5303, 0x5323, 1063282, 1060613, 80, 25, 0, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_NINJITSU, 4, 505, "Surprise Attack", nullptr, 0x5304, 0x5324, 1063283, 1060614, 30, 20, 0, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_NINJITSU, 5, 506, "Backstab", nullptr, 0x5305, 0x5325, 1063284, 1060615, 20, 30, 0, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_NINJITSU, 6, 507, "Shadowjump", nullptr, 0x5306, 0x5326, 1063285, 1060616, 50, 15, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_NINJITSU, 7, 508, "Mirror Image", nullptr, 0x5307, 0x5327, 1063286, 1060617, 40, 10, 0, TT_NEUTRAL, nullptr });
    // Spellweaving
    assert(s_Spells.size() == int(SpellOffset::Spellweaving));
    s_Spells.push_back({ ST_SPELLWEAVING, 0, 601, "Arcane Circle", "Myrshalee", 0x59d8, 0x59d8, 1072042, 1071026, 0, 24, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 1, 602, "Gift of Renewal", "Olorisstra", 0x59d9, 0x59d9, 1072043, 1071027, 0, 24, 0, TT_BENEFICIAL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 2, 603, "Immolating Weapon", "Thalshara", 0x59da, 0x59da, 1072044, 1071028, 10, 32, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 3, 604, "Attunement", "Haeldril", 0x59db, 0x59db, 1072045, 1071029, 0, 24, 0, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 4, 605, "Thunderstorm", "Erelonia", 0x59dc, 0x59dc, 1072046, 1071030, 10, 32, 0, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 5, 606, "Nature's Fury", "Rauvvrae", 0x59dd, 0x59dd, 1072047, 1071031, 0, 24, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 6, 607, "Summon Fey", "Alalithra", 0x59de, 0x59de, 1072048, 1071032, 38, 10, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 7, 608, "Summon Fiend", "Nylisstra", 0x59df, 0x59df, 1072049, 1071033, 38, 10, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 8, 609, "Reaper Form", "Tarisstree", 0x59e0, 0x59e0, 1072050, 1071034, 24, 34, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 9, 610, "Wildfire", "Haelyn", 0x59e1, 0x59e1, 1072051, 1071035, 66, 50, 0, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 10, 611, "Essence of Wind", "Anathrae", 0x59e2, 0x59e2, 1072052, 1071036, 52, 40, 0, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 11, 612, "Dryad Allure", "Rathril", 0x59e3, 0x59e3, 1072053, 1071037, 52, 40, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 12, 613, "Ethereal Voyage", "Orlavdra", 0x59e4, 0x59e4, 1072054, 1071038, 24, 32, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 13, 614, "Word of Death", "Nyraxle", 0x59e5, 0x59e5, 1072055, 1071039, 83, 50, 0, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 14, 615, "Gift of Life", "Illorae", 0x59e6, 0x59e6, 1072056, 1071040, 38, 70, 0, TT_BENEFICIAL, nullptr });
    s_Spells.push_back({ ST_SPELLWEAVING, 15, 616, "Arcane Empowerment", "Aslavdra", 0x59e7, 0x59e7, 1072057, 1071041, 24, 50, 0, TT_BENEFICIAL, nullptr });
    // Mysticism
    assert(s_Spells.size() == int(SpellOffset::Mysticism));
    s_Spells.push_back({ ST_MYSTICISM, 0, 678, "Nether Bolt", "In Corp Ylem", 0x5dc0, 0x5dc0, 1095193, 1031678, 0, 4, 0, TT_HARMFUL, "Black Pearl\nSulfurous Ash" });
    s_Spells.push_back({ ST_MYSTICISM, 1, 679, "Healing Stone", "Kal In Mani", 0x5dc1, 0x5dc1, 1095194, 1031679, 0, 4, 0, TT_NEUTRAL, "Bone\nGarlic\nGinseng\nSpiders' Silk" });
    s_Spells.push_back({ ST_MYSTICISM, 2, 680, "Purge Magic", "An Ort Sanct", 0x5dc2, 0x5dc2, 1095195, 1031680, 8, 6, 0, TT_BENEFICIAL, "Fertile Dirt\nGarlic\nMandrake Root\nSulfurous Ash" });
    s_Spells.push_back({ ST_MYSTICISM, 3, 681, "Enchant", "In Ort Ylem", 0x5dc3, 0x5dc3, 1095196, 1031681, 8, 6, 0, TT_NEUTRAL, "Spiders' Silk\nMandrake Root\nSulfurous Ash" });
    s_Spells.push_back({ ST_MYSTICISM, 4, 682, "Sleep", "In Zu", 0x5dc4, 0x5dc4, 1095197, 1031682, 20, 9, 0, TT_HARMFUL, "Nightshade\nSpiders' Silk\nBlack Pearl" });
    s_Spells.push_back({ ST_MYSTICISM, 5, 683, "Eagle Strike", "Kal Por Xen", 0x5dc5, 0x5dc5, 1095198, 1031683, 20, 9, 0, TT_HARMFUL, "Bloodmoss\nBone\nMandrake Root\nSpiders' Silk" });
    s_Spells.push_back({ ST_MYSTICISM, 6, 684, "Animated Weapon", "In Jux Por Ylem", 0x5dc6, 0x5dc6, 1095199, 1031684, 33, 11, 0, TT_NEUTRAL, "Bone\nBlack Pearl\nMandrake Root\nNightshade" });
    s_Spells.push_back({ ST_MYSTICISM, 7, 685, "Stone Form", "In Rel Ylem", 0x5dc7, 0x5dc7, 1095200, 1031685, 33, 11, 0, TT_NEUTRAL, "Bloodmoss\nFertile Dirt\nGarlic" });
    s_Spells.push_back({ ST_MYSTICISM, 8, 686, "Spell Trigger", "In Vas Ort Ex", 0x5dc8, 0x5dc8, 1095201, 1031686, 45, 14, 0, TT_NEUTRAL, "Dragon's Blood\nGarlic\nMandrake Root\nSpiders' Silk" });
    s_Spells.push_back({ ST_MYSTICISM, 9, 687, "Mass Sleep", "Vas Zu", 0x5dc9, 0x5dc9, 1095202, 1031687, 45, 14, 0, TT_HARMFUL, "Ginseng\nNightshade\nSpiders' Silk" });
    s_Spells.push_back({ ST_MYSTICISM, 10, 688, "Cleansing Winds", "In Vas Mani Hur", 0x5dca, 0x5dca, 1095203, 1031688, 58, 20, 0, TT_BENEFICIAL, "Dragon's Blood\nGarlic\nGinseng\nMandrake Root" });
    s_Spells.push_back({ ST_MYSTICISM, 11, 689, "Bombard", "Corp Por Ylem", 0x5dcb, 0x5dcb, 1095204, 1031689, 58, 20, 0, TT_HARMFUL, "Bloodmoss\nDragon's Blood\nGarlic\nSulfurous Ash" });
    s_Spells.push_back({ ST_MYSTICISM, 12, 690, "Spell Plague", "Vas Rel Jux Ort", 0x5dcc, 0x5dcc, 1095205, 1031690, 70, 40, 0, TT_HARMFUL, "Demon Bone\nDragon's Blood\nNightshade\nSulfurous Ash" });
    s_Spells.push_back({ ST_MYSTICISM, 13, 691, "Hail Storm", "Kal Des Ylem", 0x5dcd, 0x5dcd, 1095206, 1031691, 70, 50, 0, TT_HARMFUL, "Dragon's Blood\nBlack Pearl\nBloodmoss\nMandrake Root" });
    s_Spells.push_back({ ST_MYSTICISM, 14, 692, "Nether Cyclone", "Grav Hur", 0x5dce, 0x5dce, 1095207, 1031692, 83, 50, 0, TT_HARMFUL, "Mandrake Root\nNightshade\nSulfurous Ash\nBloodmoss" });
    s_Spells.push_back({ ST_MYSTICISM, 15, 693, "Rising Colossus", "Kal Vas Xen Corp Ylem", 0x5dcf, 0x5dcf, 1095208, 1031693, 83, 50, 0, TT_NEUTRAL, "Demon Bone\nDragon's Blood\nFertile Dirt\nNightshade" });
    // Mastery
    assert(s_Spells.size() == int(SpellOffset::Mastery));
    s_Spells.push_back({ ST_MASTERY, 0, 701, "Inspire", "Uus Por", 0x0945, 0x0945, 1115689, 1115612, 90, 16, 4, TT_BENEFICIAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 1, 702, "Invigorate", "An Zu", 0x0946, 0x0946, 1115690, 1115613, 90, 22, 5, TT_BENEFICIAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 2, 703, "Resilience", "Kal Mani Tym", 0x0947, 0x0947, 1115691, 1115614, 90, 16, 4, TT_BENEFICIAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 3, 704, "Perseverance", "Uus Jux Sanct", 0x0948, 0x0948, 1115692, 1115615, 90, 18, 5, TT_BENEFICIAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 4, 705, "Tribulation", "In Jux Hur Rel", 0x0949, 0x0949, 1115693, 1115616, 90, 24, 10, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 5, 706, "Despair", "Kal Des Mani Tym", 0x094a, 0x094a, 1115694, 1115617, 90, 26, 12, TT_HARMFUL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 6, 707, "Death Ray", "In Grav Corp", 0x9b8b, 0x9b8b, 1155938, 1155896, 90, 50, 35, TT_HARMFUL, "Black Pearl\nBloodmoss\nSpiders' Silk" });
    s_Spells.push_back({ ST_MASTERY, 7, 708, "Ethereal Burst", "Uus Ort Grav", 0x9b8c, 0x9b8c, 1155939, 1155897, 90, 0, 0, TT_BENEFICIAL, "Bloodmoss\nGinseng\nMandrake Root" });
    s_Spells.push_back({ ST_MASTERY, 8, 709, "Nether Blast", "In Vas Xen Por", 0x9b8d, 0x9b8d, 1155940, 1155898, 90, 40, 0, TT_HARMFUL, "Dragon's Blood\nDemon Bone" });
    s_Spells.push_back({ ST_MASTERY, 9, 710, "Mystic Weapon", "Vas Ylem Wis", 0x9b8e, 0x9b8e, 1155941, 1155899, 90, 40, 0, TT_NEUTRAL, "Fertile Dirt\nBone" });
    s_Spells.push_back({ ST_MASTERY, 10, 711, "Command Undead", "In Corp Xen Por", 0x9b8f, 0x9b8f, 1155942, 1155900, 90, 40, 0, TT_NEUTRAL, "Daemon Blood\nPig Iron\nBat Wing" });
    s_Spells.push_back({ ST_MASTERY, 11, 712, "Conduit", "Uus Corp Grav", 0x9b90, 0x9b90, 1155943, 1155901, 90, 40, 0, TT_HARMFUL, "Nox Crystal\nBat Wing\nGrave Dust" });
    s_Spells.push_back({ ST_MASTERY, 12, 713, "Mana Shield", "Faerkulggen", 0x9b91, 0x9b91, 1155944, 1155902, 90, 40, 0, TT_BENEFICIAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 13, 714, "Summon Reaper", "Lartarisstree", 0x9b92, 0x9b92, 1155945, 1155903, 90, 50, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 18, 715, "Enchanted Summoning", nullptr, 0x9b93, 0x9b93, 1155946, 1155904, 90, 0, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 19, 716, "Anticipate Hit", nullptr, 0x9b94, 0x9b94, 1155947, 1155905, 90, 10, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 16, 717, "Warcry", nullptr, 0x9b95, 0x9b95, 1155948, 1155906, 90, 40, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 20, 718, "Intuition", nullptr, 0x9b96, 0x9b96, 1155949, 1155907, 90, 0, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 21, 719, "Rejuvenate", nullptr, 0x9b97, 0x9b97, 1155950, 1155908, 90, 10, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 24, 720, "Holy Fist", nullptr, 0x9b98, 0x9b98, 1155951, 1155909, 90, VLE(CV_70500, 40, 50), 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 33, 721, "Shadow", nullptr, 0x9b99, 0x9b99, 1155952, 1155910, 90, 10, 4, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 34, 722, "White Tiger Form", nullptr, 0x9b9a, 0x9b9a, 1155953, 1155911, 90, 10, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 35, 723, "Flaming Shot", nullptr, 0x9b9b, 0x9b9b, 1155954, 1155912, 90, 30, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 26, 724, "Playing The Odds", nullptr, 0x9b9c, 0x9b9c, 1155955, 1155913, 90, VLE(CV_704565, 20, 25), 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 27, 725, "Thrust", nullptr, 0x9b9d, 0x9b9d, 1155956, 1155914, 90, VLE(CV_704565, 20, 30), 20, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 28, 726, "Pierce", nullptr, 0x9b9e, 0x9b9e, 1155957, 1155915, 90, 20, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 31, 727, "Stagger", nullptr, 0x9b9f, 0x9b9f, 1155958, 1155916, 90, 20, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 36, 728, "Toughness", nullptr, 0x9ba0, 0x9ba0, 1155959, 1155917, 90, 20, 20, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 37, 729, "Onslaught", nullptr, 0x9ba1, 0x9ba1, 1155960, 1155918, 90, 20, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 39, 730, "Focused Eye", nullptr, 0x9ba2, 0x9ba2, 1155961, 1155919, 90, 20, 20, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 40, 731, "Elemental Fury", nullptr, 0x9ba3, 0x9ba3, 1155962, 1155920, 90, 20, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 22, 732, "Called Shot", nullptr, 0x9ba4, 0x9ba4, 1155963, 1155921, 90, VLE(CV_70500, 20, 40), 0, TT_NEUTRAL, nullptr });
    if (g_Config.ClientVersion < CV_70611)
    {
        s_Spells.push_back({ ST_MASTERY, 23, 733, "Saving Throw", nullptr, 0x9ba5, 0x9ba5, 1155964, 1155922, 90, 0, 0, TT_NEUTRAL, nullptr });
    }
    else
    {
        s_Spells.push_back({ ST_MASTERY, 23, 733, "Warrior's Gifts", nullptr, 0x9ba5, 0x9ba5, 1155964, 1155922, 90, 0, 0, TT_NEUTRAL, nullptr });
    }
    s_Spells.push_back({ ST_MASTERY, 29, 734, "Shield Bash", nullptr, 0x9ba6, 0x9ba6, 1155965, 1155923, 90, VLE(CV_704565, 40, 40), 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 30, 735, "Bodyguard", nullptr, 0x9ba7, 0x9ba7, 1155966, 1155924, 90, 40, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 42, 736, "Heighten Senses", nullptr, 0x9ba8, 0x9ba8, 1155967, 1155925, 90, 10, 10, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 43, 737, "Tolerance", nullptr, 0x9ba9, 0x9ba9, 1155968, 1155926, 90, 20, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 14, 738, "Injected Strike", nullptr, 0x9baa, 0x9baa, 1155969, 1155927, 90, 30, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 15, 739, "Potency", nullptr, 0x9bab, 0x9bab, 1155970, 1155928, 90, 0, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 17, 740, "Rampage", nullptr, 0x9bac, 0x9bac, 1155971, 1155929, 90, 20, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 32, 741, "Fists of Fury", nullptr, 0x9bad, 0x9bad, 1155972, 1155930, 90, 20, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 38, 742, "Knockout", nullptr, 0x9bae, 0x9bae, 1155973, 1155931, 90, 0, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 41, 743, "Whispering", nullptr, 0x9baf, 0x9baf, 1155974, 1155932, 90, 40, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 44, 744, "Combat Training", nullptr, 0x9bb0, 0x9bb0, 1155975, 1155933, 90, 40, 0, TT_NEUTRAL, nullptr });
    s_Spells.push_back({ ST_MASTERY, 45, 745, "Boarding", nullptr, 0x9bb1, 0x9bb1, 1155976, 1155934, 90, 0, 0, TT_NEUTRAL, nullptr });
    // clang-format on

    if (g_Config.ClientVersion < CV_305D)
    {
        s_Spells[4].Reagents = "Black pearl\nNightshade";                    // Magic Arrow
        s_Spells[17].Reagents = "Black pearl\nSulfurous ash";                // Fireball
        s_Spells[29].Reagents = "Black pearl\nMandrake root\nSulfurous ash"; // Lightning
        s_Spells[42].Reagents = "Garlic\nMandrake root\nSulfurous ash";      // Explosion
    }

    if (g_Config.ClientVersion)

        for (int type = ST_FIRST; type < ST_COUNT; ++type)
        {
            SpellBook book;
            book.Type = SPELLBOOK_TYPE(type);
            switch (book.Type)
            {
                case ST_MAGERY:
                {
                    book.Graphic = 0x08aC;
                    book.MinimizedGraphic = 0x08ba;
                    book.SpellCount = int(SpellCount::Magery);
                    book.HasSeparator = true;
                    break;
                }
                case ST_NECROMANCY:
                {
                    book.Graphic = 0x2b00;
                    book.MinimizedGraphic = 0x2b03;
                    book.SpellCount = int(SpellCount::Necromancy);
                    book.HasSeparator = true;
                    break;
                }
                case ST_CHIVALRY:
                {
                    book.Graphic = 0x2b01;
                    book.MinimizedGraphic = 0x2b04;
                    book.SpellCount = int(SpellCount::Chivalry);
                    break;
                }
                case ST_BUSHIDO:
                {
                    book.Graphic = 0x2b07;
                    book.MinimizedGraphic = 0x2b09;
                    book.SpellCount = int(SpellCount::Bushido);
                    break;
                }
                case ST_NINJITSU:
                {
                    book.Graphic = 0x2B06;
                    book.MinimizedGraphic = 0x2b08;
                    book.SpellCount = int(SpellCount::Ninjitsu);
                    break;
                }
                case ST_SPELLWEAVING:
                {
                    book.Graphic = 0x2b2f;
                    book.MinimizedGraphic = 0x2b2d;
                    book.SpellCount = int(SpellCount::Spellweaving);
                    break;
                }
                case ST_MYSTICISM:
                {
                    book.Graphic = 0x2b32;
                    book.MinimizedGraphic = 0x2b30;
                    book.SpellCount = int(SpellCount::Mysticism);
                    book.HasSeparator = true;
                    break;
                }
                case ST_MASTERY:
                {
                    book.Graphic = 0x08ac;
                    book.MinimizedGraphic = 0x08ba;
                    book.SpellCount = int(SpellCount::Mastery);
                    break;
                }
                default:
                case ST_INVALID:
                case ST_COUNT:
                    assert(false && "Invalid Spellbook Type");
                    break;
            }
            book.SpellsPerPage = std::min(book.SpellCount / 2, 8);
            book.IndexPagesCount = (int)ceilf(book.SpellCount / 8.0f);
            if (type == ST_MASTERY)
            {
                book.IndexPagesCount *= 2; // Mastery layout skips a page to show masteries
            }

            if (book.IndexPagesCount % 2 == 1)
            {
                book.IndexPagesCount++;
            }

            int s = 0;
            for (const auto &spell : s_Spells)
            {
                if (spell.Type != type)
                    continue;
                book.Spells[s++] = &spell;
            }
            s_SpellBooks.emplace_back(book);
        }
}