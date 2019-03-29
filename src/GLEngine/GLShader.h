// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

void UnuseShader();

class CGLShader
{
protected:
    GLuint m_Shader{ 0 };

    GLuint m_VertexShader{ 0 };

    GLuint m_FragmentShader{ 0 };

    //Указатель на текстуру для передачи ее шейдеру (можно не использовать, текстура передается автоматически при glBingTexture)
    GLuint m_TexturePointer{ 0 };

public:
    CGLShader();
    virtual ~CGLShader();

    //Инициализировать шейдер
    virtual bool Init(const char *vertexShaderData, const char *fragmentShaderData);

    //Использовать шейдер
    virtual bool Use();

    //Остановить использование шейдера
    virtual void Pause();

    //Восстановить работу шейдера
    virtual void Resume();
};

class CDeathShader : public CGLShader
{
public:
    CDeathShader();

    //Инициализировать шейдер
    virtual bool Init(const char *vertexShaderData, const char *fragmentShaderData);
};

class CColorizerShader : public CGLShader
{
protected:
    //Указатель на палитру цветов для передачи ее шейдеру
    GLuint m_ColorTablePointer{ 0 };

    //Указатель на метод вывода шейдером
    GLuint m_DrawModePointer{ 0 };

public:
    CColorizerShader();

    //Инициализировать шейдер
    virtual bool Init(const char *vertexShaderData, const char *fragmentShaderData);

    //Использовать шейдер
    virtual bool Use();
};

extern CDeathShader g_DeathShader;
extern CColorizerShader g_ColorizerShader;
extern CColorizerShader g_FontColorizerShader;
extern CColorizerShader g_LightColorizerShader;
