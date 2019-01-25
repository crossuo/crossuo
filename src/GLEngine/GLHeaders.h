#pragma once

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
