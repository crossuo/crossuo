/***********************************************************************************
**
** ToolTip.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef TOOLTIP_H
#define TOOLTIP_H

//Класс для работы с тултипами
class CToolTip
{
public:
    uint32_t Timer = 0;
    wstring Data = L"";
    uint32_t ClilocID = 0;
    int MaxWidth = 0;
    Wisp::CPoint2Di Position = Wisp::CPoint2Di();
    bool Use = false;

    CToolTip();
    ~CToolTip();

    void Reset();

    class CRenderObject *m_Object{ nullptr };

    //Текстура тулпита
    CGLTextTexture Texture;

    void CreateTextTexture(CGLTextTexture &texture, const wstring &str, int &width, int minWidth);

    //Установить тултип из строки
    void Set(const wstring &str, int maxWidth = 0);

    //Установить тултип из клилока
    void Set(int clilocID, const string &str, int maxWidth = 0, bool toCamelCase = false);

    //Отрисовать тултип
    void Draw(int cursorWidth = 0, int cursorHeight = 0);
};

extern CToolTip g_ToolTip;

#endif
