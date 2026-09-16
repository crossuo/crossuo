// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (macros/macro.h), trimmed: the CMacro object
// hierarchy is unchanged; the XML-writer Save / XML-attribute
// Load signatures became the config/xml DOM equivalents (identical attribute
// names and value formatting). The executor stays with the Phase 3 UI
// (master plan: port the MODEL this round).

#pragma once

#include "core/global.h"
#include "config/xml.h"

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

    CMacro(const MACRO_TYPE &type)
        : Type(type)
    {
    }
    CMacro(CMacro *macro)
        : Type(macro->Type)
        , ConvertType(macro->ConvertType)
        , WaitingTime(macro->WaitingTime)
    {
    }
    virtual ~CMacro() {}

    astr_t GetText();

    bool HaveConvertTable();

    virtual CMacro *Copy();

    virtual void Save(XMLElement &element);

    static CMacro *Load(const XMLElement &element);
};

class CMacroWithIntValue : public CMacro
{
    virtual MACRO_CLASS GetClass() const { return MCLASS_INT_VALUE; }

public:
    int Value = 0;

    CMacroWithIntValue(const MACRO_TYPE &type, const int &value)
        : CMacro(type)
        , Value(value)
    {
    }
    CMacroWithIntValue(CMacroWithIntValue *macro)
        : CMacro(macro)
        , Value(macro->Value)
    {
    }
    virtual ~CMacroWithIntValue() {}

    virtual CMacro *Copy();

    virtual void Save(XMLElement &element);
};

class CMacroWithStringValue : public CMacro
{
    virtual MACRO_CLASS GetClass() const { return MCLASS_STRING_VALUE; }

public:
    astr_t Value = "";

    CMacroWithStringValue(const MACRO_TYPE &type, const astr_t &value)
        : CMacro(type)
        , Value(value)
    {
    }
    CMacroWithStringValue(CMacroWithStringValue *macro)
        : CMacro(macro)
        , Value(macro->Value)
    {
    }
    virtual ~CMacroWithStringValue() {}

    virtual CMacro *Copy();

    virtual void Save(XMLElement &element);
};

class CMacroWithObject : public CMacro
{
    virtual MACRO_CLASS GetClass() const { return MCLASS_OBJECT; }

public:
    uint Serial = 0;
    ushort Graphic = 0;
    ushort Color = 0;

    CMacroWithObject(MACRO_TYPE type, uint serial, ushort graphic, ushort color)
        : CMacro(type)
        , Serial(serial)
        , Graphic(graphic)
        , Color(color)
    {
    }
    CMacroWithObject(CMacroWithObject *macro)
        : CMacro(macro)
        , Serial(macro->Serial)
        , Graphic(macro->Graphic)
        , Color(macro->Color)
    {
    }
    virtual ~CMacroWithObject() {}

    virtual CMacro *Copy();

    virtual void Save(XMLElement &element);
};

class CMacroWithGump : public CMacro
{
    virtual MACRO_CLASS GetClass() const { return MCLASS_GUMP; }

public:
    uint Serial = 0;
    uint ID = 0;
    uint Button = 0;

    CMacroWithGump(const MACRO_TYPE &type, const uint &serial, const uint &id, const uint &button)
        : CMacro(type)
        , Serial(serial)
        , ID(id)
        , Button(button)
    {
    }
    CMacroWithGump(CMacroWithGump *macro)
        : CMacro(macro)
        , Serial(macro->Serial)
        , ID(macro->ID)
        , Button(macro->Button)
    {
    }
    virtual ~CMacroWithGump() {}

    virtual CMacro *Copy();

    virtual void Save(XMLElement &element);
};

class CMacroWithMenu : public CMacro
{
    virtual MACRO_CLASS GetClass() const { return MCLASS_MENU; }

public:
    astr_t Name = "";
    astr_t ItemName = "";
    ushort Graphic = 0;
    ushort Color = 0;

    CMacroWithMenu(
        const MACRO_TYPE &type,
        const astr_t &name,
        const astr_t &itemName,
        const ushort &graphic,
        const ushort &color)
        : CMacro(type)
        , Name(name)
        , ItemName(itemName)
        , Graphic(graphic)
        , Color(color)
    {
    }
    CMacroWithMenu(CMacroWithMenu *macro)
        : CMacro(macro)
        , Name(macro->Name)
        , ItemName(macro->ItemName)
        , Graphic(macro->Graphic)
        , Color(macro->Color)
    {
    }
    virtual ~CMacroWithMenu() {}

    virtual CMacro *Copy();

    virtual void Save(XMLElement &element);
};

class CMacroWithTargetTile : public CMacro
{
    virtual MACRO_CLASS GetClass() const { return MCLASS_TARGET_TILE; }

public:
    ushort Graphic = 0;
    int X = 0;
    int Y = 0;
    int Z = 0;
    int RelativeX = 0;
    int RelativeY = 0;
    int RelativeZ = 0;

    CMacroWithTargetTile(
        const MACRO_TYPE &type,
        const ushort &graphic,
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
    CMacroWithTargetTile(CMacroWithTargetTile *macro)
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
    virtual ~CMacroWithTargetTile() {}

    virtual CMacro *Copy();

    virtual void Save(XMLElement &element);
};

} // namespace xuoassist
