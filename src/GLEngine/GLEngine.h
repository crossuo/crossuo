// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../Point.h"

typedef std::deque<CRect> SCISSOR_LIST;

struct CGLTexture;
class CGLEngine
{
public:
    HDC DC = 0;
    HGLRC RC = 0;
    GLuint OldTexture = 0;
    bool Drawing = false;
    bool CanUseFrameBuffer = false;
#ifndef NEW_RENDERER_ENABLED
    float SittingCharacterOffset = 8.0f;
#endif
    GLuint PositionBuffer = 0;
    SDL_GLContext m_context;

private:
    SCISSOR_LIST m_ScissorList;

public:
    CGLEngine();
    virtual ~CGLEngine();

    //Инициализация OpenGL
    bool Install();

    //Завершение работы с OpenGL
    void Uninstall();

    //Обновление области вывода
    void UpdateRect();

    //Очистить экран и начать рисование
    void BeginDraw();

    //Завершить рисование и вывести все нарисованное на экран
    void EndDraw();

    //Включить буфер трафарета
    void BeginStencil();

    //Выключить буфер трафарета
    void EndStencil();

    //Указать область рисования
    void ViewPortScaled(int x, int y, int width, int height);

    //Указать область рисования
    void ViewPort(int x, int y, int width, int height);

    //Восстановить стандартную область рисования
    void RestorePort();

    //Указать область рисования (ножницами, сохраняет мартицу)
    void PushScissor(int x, int y, int width, int height);
    void PushScissor(const CPoint2Di &position, int width, int height);
    void PushScissor(int x, int y, const CSize &size);
    void PushScissor(const CPoint2Di &position, const CSize &size);
    void PushScissor(const CRect &rect);

    void PopScissor();

    void ClearScissorList();

#ifndef NEW_RENDERER_ENABLED
    inline void BindTexture(GLuint texture);
#endif

    //Нарисовать линию
    void DrawLine(int x, int y, int targetX, int targetY);

    //Нарисовать прямоугольник
    void DrawPolygone(int x, int y, int width, int height);

#ifndef NEW_RENDERER_ENABLED
    void DrawCircle(float x, float y, float radius, int gradientMode = 0);
    void BindTexture16(CGLTexture &texture, int width, int height, uint16_t *pixels);
    void BindTexture32(CGLTexture &texture, int width, int height, uint32_t *pixels);
    void DrawLandTexture(const CGLTexture &texture, int x, int y, CLandObject *land);
    void Draw(const CGLTexture &texture, int x, int y);
    void DrawRotated(const CGLTexture &texture, int x, int y, float angle);
    void DrawMirrored(const CGLTexture &texture, int x, int y, bool mirror);
    void DrawSitting(
        const CGLTexture &texture,
        int x,
        int y,
        bool mirror,
        float h3mod,
        float h6mod,
        float h9mod);
    void DrawShadow(const CGLTexture &texture, int x, int y, bool mirror);
    void DrawStretched(const CGLTexture &texture, int x, int y, int drawWidth, int drawHeight);
    void DrawResizepic(CGLTexture **th, int x, int y, int width, int height);
#endif
};

extern CGLEngine g_GL;
