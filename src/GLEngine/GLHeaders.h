// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#define NO_SDL_GLEXT
#include "GL/glew.h"

#if defined(XUO_WINDOWS)
#include "GL/wglew.h"
#else
#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#endif // XUO_WINDOWS
