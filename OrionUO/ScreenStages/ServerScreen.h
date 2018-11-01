/***********************************************************************************
**
** ServerScreen.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#pragma once
#include "Input.h"

class CServerScreen : public CBaseScreen
{
public:
    //!Временная переменная для идентификации выбранного сервера
    int SelectionServerTempValue = 0;

private:
    CGumpScreenServer m_ServerGump;

public:
    CServerScreen();
    virtual ~CServerScreen();

    //!Идентификаторы событий для плавного перехода
    static const uint8_t ID_SMOOTH_SS_QUIT = 1;
    static const uint8_t ID_SMOOTH_SS_SELECT_SERVER = 2;
    static const uint8_t ID_SMOOTH_SS_GO_SCREEN_MAIN = 3;

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

    virtual void OnKeyDown(const KeyEvent &ev) override;
};

extern CServerScreen g_ServerScreen;
