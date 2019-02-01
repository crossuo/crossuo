// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"
#include "../Platform.h"

class CGumpOptions : public CGump
{
private:
    uint32_t m_LastChangeMacroTime = 0;

    void DrawPage1();  //Sound and Music
    void DrawPage2();  //Pop-up Help
    void DrawPage3();  //Language
    void DrawPage4();  //Chat
    void DrawPage5();  //Macro Options
    void DrawPage6();  //Interface
    void DrawPage7();  //Display
    void DrawPage8();  //Reputation System
    void DrawPage9();  //Miscellaneous
    void DrawPage10(); //Filter Options

    void ApplyPageChanges();

    class CMacro *m_MacroPointer{ nullptr };
    class CMacroObject *m_MacroObjectPointer{ nullptr };

    CGUISlider *m_SliderSound{ nullptr };
    CGUISlider *m_SliderMusic{ nullptr };
    CGUISlider *m_SliderClientFPS{ nullptr };
    CGUISlider *m_SliderHiddenAlpha{ nullptr };
    CGUISlider *m_SliderSpellIconsAlpha{ nullptr };
    CGUISlider *m_SliderDrawStatusConditionValue{ nullptr };
    CGUISlider *m_SliderTooltipDelay{ nullptr };
    CGUISlider *m_SliderSpeechDuration{ nullptr };
    CGUISlider *m_SliderCircleTransparency{ nullptr };
    CGUISlider *m_SliderInformSkills{ nullptr };

#if USE_PING
    CGUISlider *m_SliderPingTimer{ nullptr };
#endif // USE_PING

    CGUIColoredPolygone *m_ColorTooltipText{ nullptr };

    CGUIColoredPolygone *m_ColorInputText{ nullptr };
    CGUIColoredPolygone *m_ColorMenuOption{ nullptr };
    CGUIColoredPolygone *m_ColorPlayerColorInMemberList{ nullptr };
    CGUIColoredPolygone *m_ColorChatText{ nullptr };
    CGUIColoredPolygone *m_ColorPlayerNameWithoutSpeakingPrivileges{ nullptr };
    CGUIColoredPolygone *m_ColorMutedText{ nullptr };
    CGUIColoredPolygone *m_ColorChannelModeratorName{ nullptr };
    CGUIColoredPolygone *m_ColorChannelModeratorText{ nullptr };
    CGUIColoredPolygone *m_ColorMyName{ nullptr };
    CGUIColoredPolygone *m_ColorMyText{ nullptr };
    CGUIColoredPolygone *m_ColorSystemMessage{ nullptr };
    CGUIColoredPolygone *m_ColorTextOutputBackground{ nullptr };
    CGUIColoredPolygone *m_ColorTextInputBackground{ nullptr };
    CGUIColoredPolygone *m_ColorUserListBackground{ nullptr };
    CGUIColoredPolygone *m_ColorConferenceListBackground{ nullptr };
    CGUIColoredPolygone *m_ColorCommandListBackground{ nullptr };

    CGUIColoredPolygone *m_ColorSpeech{ nullptr };
    CGUIColoredPolygone *m_ColorEmote{ nullptr };
    CGUIColoredPolygone *m_ColorPartyMessage{ nullptr };
    CGUIColoredPolygone *m_ColorGuildMessage{ nullptr };
    CGUIColoredPolygone *m_ColorAllianceMessage{ nullptr };

    CGUIColoredPolygone *m_ColorInnocent{ nullptr };
    CGUIColoredPolygone *m_ColorFriendly{ nullptr };
    CGUIColoredPolygone *m_ColorSomeone{ nullptr };
    CGUIColoredPolygone *m_ColorCriminal{ nullptr };
    CGUIColoredPolygone *m_ColorEnemy{ nullptr };
    CGUIColoredPolygone *m_ColorMurderer{ nullptr };

    CGUIDataBox *m_MacroDataBox{ nullptr };
    CGUICheckbox *m_MacroCheckboxShift{ nullptr };
    CGUICheckbox *m_MacroCheckboxAlt{ nullptr };
    CGUICheckbox *m_MacroCheckboxCtrl{ nullptr };

    CGUITextEntry *m_ContainerOffsetX{ nullptr };
    CGUITextEntry *m_ContainerOffsetY{ nullptr };

    bool m_WantRedrawMacroData = true;
    void RedrawMacroData();

protected:
    virtual void CalculateGumpState();

public:
    CGumpOptions(short x, short y);
    virtual ~CGumpOptions();

    CGUITextEntry *m_GameWindowWidth{ nullptr };
    CGUITextEntry *m_GameWindowHeight{ nullptr };
    CGUITextEntry *m_MacroKey{ nullptr };

    void UpdateColor(const SELECT_COLOR_GUMP_STATE &state, uint16_t color);
    virtual void InitToolTip();
    virtual void PrepareContent();
    virtual void UpdateContent();
    void Init();

    GUMP_BUTTON_EVENT_H;
    GUMP_CHECKBOX_EVENT_H;
    GUMP_RADIO_EVENT_H;
    GUMP_COMBOBOX_SELECTION_EVENT_H;
    GUMP_SLIDER_CLICK_EVENT_H;
    GUMP_SLIDER_MOVE_EVENT_H;

    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};
