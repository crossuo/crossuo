// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (macros/macro.h), trimmed: the CMacro object
// hierarchy is unchanged; the XML-writer Save / XML-attribute Load
// signatures became the config INI equivalents (identical key names and
// value formatting). The executor stays with the Phase 3 UI
// (master plan: port the MODEL this round).

#pragma once

#include "core/global.h"
#include "config/configio.h"

namespace xuoassist {

enum MACRO_TYPE
{
    MT_NONE = 0,
    MT_USE,
    MT_ATTACK,
    MT_WAIT_FOR_TARGET_OBJECT,
    MT_WAIT_FOR_TARGET_TILE,
    MT_USE_SKILL,
    MT_CAST,
    MT_CAST_FROM_BOOK,
    MT_CAST_NEW,
    MT_SAY,
    MT_PICK_UP_ITEM,
    MT_DROP_ITEM,
    MT_EQUIP_ITEM,
    MT_WAIT_FOR_PROMPT,
    MT_WAIT_FOR_MENU,
    MT_WAIT_FOR_GUMP,
    MT_REQUEST_CONTEXT_MENU,
    MT_WAIT_FOR_CONTEXT_MENU,
    MT_SELECT_TRADE,
    MT_WAR_MODE,
    MT_WALK,
    MT_HELP_MENU_REQUEST,
    MT_QUEST_MENU_REQUEST,
    MT_GUILD_MENU_REQUEST,
    MT_WRESTLING_STUN,
    MT_WRESTLING_DISARM,
    MT_COMBAT_ABILITY,
    MT_TOGGLE_GARGOYLE_FLYING,

    MT_WAIT = 1000,
    MT_SET_ARM,
    MT_DISARM,
    MT_ARM,
    MT_SET_DRESS,
    MT_UNDRESS,
    MT_DRESS
};

enum MACRO_CONVERT_TYPE
{
    MCT_NONE = 0,
    MCT_BY_TYPE,
    MCT_BY_TYPE_COLOR,
    MCT_BY_TYPE_GROUND,
    MCT_BY_TYPE_COLOR_GROUND,
    MCT_BY_TARGET_RELATIVE,
    MCT_BY_DROP_POSITION,
    MCT_TOGGLE,
    MCT_LAST,
    MCT_SELF,
    MCT_LASTATTACK,
    MCT_LASTTARGET,
    MCT_FRIEND,
    MCT_ENEMY,
    MCT_ANY,
    MCT_SAY,
    MCT_SAY_YELL,
    MCT_SAY_WHISPER,
    MCT_SAY_EMOTE,
    MCT_SAY_BROADCAST,
    MCT_SAY_PARTY,
    MCT_SAY_GUILD,
    MCT_SAY_ALLIANCE,
    MCT_LASTCONTAINER,
    MCT_BACKPACK,
    MCT_TURN,
    MCT_WALK,
    MCT_RUN,
    MCT_PRIMARY,
    MCT_SECONDARY,
    MCT_COUNT_OF_ITEMS
};

enum MACRO_EDIT_FLAGS
{
    MEF_NONE = 0,
    MEF_WAIT_SET = 0x20000000,
    MEF_WAIT_ADD = 0x40000000,
    MEF_EDIT = 0x80000000
};

class CMacro
{
public:
    MACRO_TYPE Type = MT_NONE;
    MACRO_CONVERT_TYPE ConvertType = MCT_NONE;
    int WaitingTime = 1000;

    // Class-kind tag replacing the original code's dynamic_cast (the client builds
    // with -fno-rtti); each subclass reports its kind and the GetText()/copy
    // paths static_cast after checking it, exactly where the original
    // dynamic_cast checks would have failed.
    enum MACRO_CLASS
    {
        MCLASS_BASE,
        MCLASS_INT_VALUE,
        MCLASS_STRING_VALUE,
        MCLASS_OBJECT,
        MCLASS_GUMP,
        MCLASS_MENU,
        MCLASS_TARGET_TILE
    };

    virtual MACRO_CLASS GetClass() const { return MCLASS_BASE; }

    explicit CMacro(const MACRO_TYPE &type)
        : Type(type)
    {
    }
    explicit CMacro(CMacro *macro)
        : Type(macro->Type)
        , ConvertType(macro->ConvertType)
        , WaitingTime(macro->WaitingTime)
    {
    }
    virtual ~CMacro() = default;

    astr_t GetText() const;

    bool HaveConvertTable() const;

    virtual CMacro *Copy();

    virtual void Save(CIniFile &ini, const astr_t &section);

    static CMacro *Load(const CIniFile &ini, const astr_t &section);
};

class CMacroWithIntValue : public CMacro
{
    MACRO_CLASS GetClass() const override { return MCLASS_INT_VALUE; }

public:
    int Value = 0;

    CMacroWithIntValue(const MACRO_TYPE &type, const int &value)
        : CMacro(type)
        , Value(value)
    {
    }
    explicit CMacroWithIntValue(CMacroWithIntValue *macro)
        : CMacro(macro)
        , Value(macro->Value)
    {
    }
    ~CMacroWithIntValue() override = default;

    CMacro *Copy() override;

    void Save(CIniFile &ini, const astr_t &section) override;
};

class CMacroWithStringValue : public CMacro
{
    MACRO_CLASS GetClass() const override { return MCLASS_STRING_VALUE; }

public:
    astr_t Value = "";

    CMacroWithStringValue(const MACRO_TYPE &type, const astr_t &value)
        : CMacro(type)
        , Value(value)
    {
    }
    explicit CMacroWithStringValue(CMacroWithStringValue *macro)
        : CMacro(macro)
        , Value(macro->Value)
    {
    }
    ~CMacroWithStringValue() override = default;

    CMacro *Copy() override;

    void Save(CIniFile &ini, const astr_t &section) override;
};

class CMacroWithObject : public CMacro
{
    MACRO_CLASS GetClass() const override { return MCLASS_OBJECT; }

public:
    uint32_t Serial = 0;
    uint16_t Graphic = 0;
    uint16_t Color = 0;

    CMacroWithObject(MACRO_TYPE type, uint32_t serial, uint16_t graphic, uint16_t color)
        : CMacro(type)
        , Serial(serial)
        , Graphic(graphic)
        , Color(color)
    {
    }
    explicit CMacroWithObject(CMacroWithObject *macro)
        : CMacro(macro)
        , Serial(macro->Serial)
        , Graphic(macro->Graphic)
        , Color(macro->Color)
    {
    }
    ~CMacroWithObject() override = default;

    CMacro *Copy() override;

    void Save(CIniFile &ini, const astr_t &section) override;
};

class CMacroWithGump : public CMacro
{
    MACRO_CLASS GetClass() const override { return MCLASS_GUMP; }

public:
    uint32_t Serial = 0;
    uint32_t ID = 0;
    uint32_t Button = 0;

    CMacroWithGump(const MACRO_TYPE &type, const uint32_t &serial, const uint32_t &id, const uint32_t &button)
        : CMacro(type)
        , Serial(serial)
        , ID(id)
        , Button(button)
    {
    }
    explicit CMacroWithGump(CMacroWithGump *macro)
        : CMacro(macro)
        , Serial(macro->Serial)
        , ID(macro->ID)
        , Button(macro->Button)
    {
    }
    ~CMacroWithGump() override = default;

    CMacro *Copy() override;

    void Save(CIniFile &ini, const astr_t &section) override;
};

class CMacroWithMenu : public CMacro
{
    MACRO_CLASS GetClass() const override { return MCLASS_MENU; }

public:
    astr_t Name = "";
    astr_t ItemName = "";
    uint16_t Graphic = 0;
    uint16_t Color = 0;

    CMacroWithMenu(
        const MACRO_TYPE &type,
        const astr_t &name,
        const astr_t &itemName,
        const uint16_t &graphic,
        const uint16_t &color)
        : CMacro(type)
        , Name(name)
        , ItemName(itemName)
        , Graphic(graphic)
        , Color(color)
    {
    }
    explicit CMacroWithMenu(CMacroWithMenu *macro)
        : CMacro(macro)
        , Name(macro->Name)
        , ItemName(macro->ItemName)
        , Graphic(macro->Graphic)
        , Color(macro->Color)
    {
    }
    ~CMacroWithMenu() override = default;

    CMacro *Copy() override;

    void Save(CIniFile &ini, const astr_t &section) override;
};

class CMacroWithTargetTile : public CMacro
{
    MACRO_CLASS GetClass() const override { return MCLASS_TARGET_TILE; }

public:
    uint16_t Graphic = 0;
    int X = 0;
    int Y = 0;
    int Z = 0;
    int RelativeX = 0;
    int RelativeY = 0;
    int RelativeZ = 0;

    CMacroWithTargetTile(
        const MACRO_TYPE &type,
        const uint16_t &graphic,
        const int &x,
        const int &y,
        const int &z,
        const int &relativeX,
        const int &relativeY,
        const int &relativeZ)
        : CMacro(type)
        , Graphic(graphic)
        , X(x)
        , Y(y)
        , Z(z)
        , RelativeX(relativeX)
        , RelativeY(relativeY)
        , RelativeZ(relativeZ)
    {
    }
    explicit CMacroWithTargetTile(CMacroWithTargetTile *macro)
        : CMacro(macro)
        , Graphic(macro->Graphic)
        , X(macro->X)
        , Y(macro->Y)
        , Z(macro->Z)
        , RelativeX(macro->RelativeX)
        , RelativeY(macro->RelativeY)
        , RelativeZ(macro->RelativeZ)
    {
    }
    ~CMacroWithTargetTile() override = default;

    CMacro *Copy() override;

    void Save(CIniFile &ini, const astr_t &section) override;
};

} // namespace xuoassist
