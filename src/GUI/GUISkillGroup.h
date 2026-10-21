// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "BaseGUI.h"
class CGUIButton;
class CGUITextEntry;

class CGUISkillGroup : public CBaseGUI
{
    //! Whether the group is collapsed or expanded
protected:
    bool m_Minimized = false;

public:
    bool GetMinimized() { return m_Minimized; };
    void SetMinimized(bool val);

public:
    CGUISkillGroup(int serial, int minimizeSerial, class CSkillGroupObject *group, int x, int y);
    virtual ~CGUISkillGroup() override;

    //! Control components
    CGUIButton *m_Minimizer{ nullptr };
    CGUITextEntry *m_Name{ nullptr };

    void UpdateDataPositions();

    virtual CSize GetSize() override;

    virtual void PrepareTextures() override;

    virtual bool EntryPointerHere() override;

    virtual void Draw(bool checktrans = false) override;
    virtual bool Select() override;

    //! Get a pointer to the selected component, or to this object
    CBaseGUI *SelectedItem();
};
