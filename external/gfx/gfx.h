#ifndef EXTERNAL_GFX_H
#define EXTERNAL_GFX_H

#if defined(XUO_WINDOWS)
#include "../../Dependencies/include/glew.h"
#include "../../Dependencies/include/wglew.h"
#else
#define NO_SDL_GLEXT
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif // XUO_WINDOWS

#include <SDL.h>

#define SOKOL_IMGUI_NO_SOKOL_APP
//#define SOKOL_GLCORE33
//#define SOKOL_GLES2
#define SOKOL_GLES3
//#define SOKOL_D3D11
//#define SOKOL_METAL
//#define SOKOL_DUMMY_BACKEND
//SOKOL_TRACE_HOOKS // sokol_gfx_imgui debugging helper

#endif // #ifndef EXTERNAL_GFX_H
