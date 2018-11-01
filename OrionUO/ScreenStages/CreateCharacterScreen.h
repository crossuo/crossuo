/***********************************************************************************
**
** CreateCharacterScreen.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#pragma once
#include "Input.h"

class CCreateCharacterScreen : public CBaseScreen
{
    //!Выбранный стиль прически/бороды
protected:
    int m_StyleSelection = 0;

public:
    int GetStyleSelection() { return m_StyleSelection; };
    void SetStyleSelection(int val);

    //!Выбранный цвет тела/одежды/волосеного покрова
protected:
    int m_ColorSelection = 0;

public:
    int GetColorSelection() { return m_ColorSelection; };
    void SetColorSelection(int val);
    string Name = "";

private:
    CGumpScreenCreateCharacter m_CreateCharacterGump;

public:
    CCreateCharacterScreen();
    virtual ~CCreateCharacterScreen();

    //!Идентификаторы событий для плавного перехода
    static const uint8_t ID_SMOOTH_CCS_QUIT = 1;
    static const uint8_t ID_SMOOTH_CCS_GO_SCREEN_CHARACTER = 2;
    static const uint8_t ID_SMOOTH_CCS_GO_SCREEN_CONNECT = 3;
    static const uint8_t ID_SMOOTH_CCS_GO_SCREEN_SELECT_TOWN = 4;

    /*!
	Инициализация
	@return 
	*/
    void Init();

    /*!
	Обработка события после плавного затемнения экрана
	@param [__in_opt] action Идентификатор действия
	@return 
	*/
    void ProcessSmoothAction(uint8_t action = 0xFF);

    virtual void OnLeftMouseButtonDown();

#if USE_WISP
    void OnCharPress(const WPARAM &wParam, const LPARAM &lParam);
#else
    virtual void OnTextInput(const SDL_TextInputEvent &ev) override;
#endif
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

extern CCreateCharacterScreen g_CreateCharacterScreen;
