// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (macros/macro.h), Qt-diet: the CMacro object
// hierarchy is unchanged; the QXmlStreamWriter Save / QXmlStreamAttributes
// Load signatures became the config/xml DOM equivalents (identical attribute
// names and value formatting). The executor stays with the Phase 3 UI
// (master plan: port the MODEL this round).

#ifndef XUOASSIST_MACROS_MACRO_H
#define XUOASSIST_MACROS_MACRO_H

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
    SETGET(MACRO_TYPE, Type, MT_NONE)
    SETGET(MACRO_CONVERT_TYPE, ConvertType, MCT_NONE)
    SETGET(int, WaitingTime, 1000)

public:
    // Class-kind tag replacing the Qt code's dynamic_cast (the client builds
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
        : m_Type(type)
    {
    }
    CMacro(CMacro *macro)
        : m_Type(macro->GetType())
        , m_ConvertType(macro->GetConvertType())
        , m_WaitingTime(macro->GetWaitingTime())
    {
    }
    virtual ~CMacro() {}

    string GetText();

    bool HaveConvertTable();

    virtual CMacro *Copy();

    virtual void Save(XMLElement &element);

    static CMacro *Load(const XMLElement &element);
};

class CMacroWithIntValue : public CMacro
{
    virtual MACRO_CLASS GetClass() const { return MCLASS_INT_VALUE; }

    SETGET(int, Value, 0)

public:
    CMacroWithIntValue(const MACRO_TYPE &type, const int &value)
        : CMacro(type)
        , m_Value(value)
    {
    }
    CMacroWithIntValue(CMacroWithIntValue *macro)
        : CMacro(macro)
        , m_Value(macro->GetValue())
    {
    }
    virtual ~CMacroWithIntValue() {}

    virtual CMacro *Copy();

    virtual void Save(XMLElement &element);
};

class CMacroWithStringValue : public CMacro
{
    virtual MACRO_CLASS GetClass() const { return MCLASS_STRING_VALUE; }

    SETGET(string, Value, "")

public:
    CMacroWithStringValue(const MACRO_TYPE &type, const string &value)
        : CMacro(type)
        , m_Value(value)
    {
    }
    CMacroWithStringValue(CMacroWithStringValue *macro)
        : CMacro(macro)
        , m_Value(macro->GetValue())
    {
    }
    virtual ~CMacroWithStringValue() {}

    virtual CMacro *Copy();

    virtual void Save(XMLElement &element);
};

class CMacroWithObject : public CMacro
{
    virtual MACRO_CLASS GetClass() const { return MCLASS_OBJECT; }

    SETGET(uint, Serial, 0)
    SETGET(ushort, Graphic, 0)
    SETGET(ushort, Color, 0)

public:
    CMacroWithObject(MACRO_TYPE type, uint serial, ushort graphic, ushort color)
        : CMacro(type)
        , m_Serial(serial)
        , m_Graphic(graphic)
        , m_Color(color)
    {
    }
    CMacroWithObject(CMacroWithObject *macro)
        : CMacro(macro)
        , m_Serial(macro->GetSerial())
        , m_Graphic(macro->GetGraphic())
        , m_Color(macro->GetColor())
    {
    }
    virtual ~CMacroWithObject() {}

    virtual CMacro *Copy();

    virtual void Save(XMLElement &element);
};

class CMacroWithGump : public CMacro
{
    virtual MACRO_CLASS GetClass() const { return MCLASS_GUMP; }

    SETGET(uint, Serial, 0)
    SETGET(uint, ID, 0)
    SETGET(uint, Button, 0)

public:
    CMacroWithGump(const MACRO_TYPE &type, const uint &serial, const uint &id, const uint &button)
        : CMacro(type)
        , m_Serial(serial)
        , m_ID(id)
        , m_Button(button)
    {
    }
    CMacroWithGump(CMacroWithGump *macro)
        : CMacro(macro)
        , m_Serial(macro->GetSerial())
        , m_ID(macro->GetID())
        , m_Button(macro->GetButton())
    {
    }
    virtual ~CMacroWithGump() {}

    virtual CMacro *Copy();

    virtual void Save(XMLElement &element);
};

class CMacroWithMenu : public CMacro
{
    virtual MACRO_CLASS GetClass() const { return MCLASS_MENU; }

    SETGET(string, Name, "")
    SETGET(string, ItemName, "")
    SETGET(ushort, Graphic, 0)
    SETGET(ushort, Color, 0)

public:
    CMacroWithMenu(
        const MACRO_TYPE &type,
        const string &name,
        const string &itemName,
        const ushort &graphic,
        const ushort &color)
        : CMacro(type)
        , m_Name(name)
        , m_ItemName(itemName)
        , m_Graphic(graphic)
        , m_Color(color)
    {
    }
    CMacroWithMenu(CMacroWithMenu *macro)
        : CMacro(macro)
        , m_Name(macro->GetName())
        , m_ItemName(macro->GetItemName())
        , m_Graphic(macro->GetGraphic())
        , m_Color(macro->GetColor())
    {
    }
    virtual ~CMacroWithMenu() {}

    virtual CMacro *Copy();

    virtual void Save(XMLElement &element);
};

class CMacroWithTargetTile : public CMacro
{
    virtual MACRO_CLASS GetClass() const { return MCLASS_TARGET_TILE; }

    SETGET(ushort, Graphic, 0)
    SETGET(int, X, 0)
    SETGET(int, Y, 0)
    SETGET(int, Z, 0)
    SETGET(int, RelativeX, 0)
    SETGET(int, RelativeY, 0)
    SETGET(int, RelativeZ, 0)

public:
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
        , m_Graphic(graphic)
        , m_X(x)
        , m_Y(y)
        , m_Z(z)
        , m_RelativeX(relativeX)
        , m_RelativeY(relativeY)
        , m_RelativeZ(relativeZ)
    {
    }
    CMacroWithTargetTile(CMacroWithTargetTile *macro)
        : CMacro(macro)
        , m_Graphic(macro->GetGraphic())
        , m_X(macro->GetX())
        , m_Y(macro->GetY())
        , m_Z(macro->GetZ())
        , m_RelativeX(macro->GetRelativeX())
        , m_RelativeY(macro->GetRelativeY())
        , m_RelativeZ(macro->GetRelativeZ())
    {
    }
    virtual ~CMacroWithTargetTile() {}

    virtual CMacro *Copy();

    virtual void Save(XMLElement &element);
};

} // namespace xuoassist

#endif // XUOASSIST_MACROS_MACRO_H
