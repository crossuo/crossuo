# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

set(PROJECT_NAME external_gfx)
project(${PROJECT_NAME})

set(EXTERNALGFX_PROJECT "${PROJECT_NAME}" CACHE INTERNAL "")
set(EXTERNALGFX_DIR "${PROJECT_SOURCE_DIR}" CACHE INTERNAL "")
set(EXTERNALGFX_INCLUDE "${CMAKE_SOURCE_DIR}" CACHE INTERNAL "")

file(GLOB EXTERNALGFX_HEADERS ${EXTERNALGFX_DIR}/*.h ${EXTERNALGFX_DIR}/imgui/*.h ${EXTERNALGFX_DIR}/imgui/bui/*.h)

set(EXTERNALGFX_SRC
    gfxpp.cpp
    gfx.c
    glctx.c
    shaderbuilder.c
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_impl_sdl3.cpp
    imgui/imgui_impl_opengl3.cpp
    imgui/imgui_stdlib.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_spinner.cpp
    imgui/bui/imgui_bui_style.cpp
    ui.cpp
)

if (USE_GL2 OR USE_GL1)
    list(APPEND EXTERNALGFX_SRC GL/glew.c)
endif()

# The DirectX11 backend is Windows-only (it includes <d3d11.h> unconditionally).
if(WIN32)
    list(APPEND EXTERNALGFX_SRC imgui/imgui_impl_dx11.cpp)
endif()

add_library(${PROJECT_NAME} STATIC ${EXTERNALGFX_SRC} ${EXTERNALGFX_HEADERS})
target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIR})

if (RENDER_AUTO)
    # The automatic binary may run on either context: imgui takes the ES3
    # backend (which also serves plain OpenGL 3.2, it only selects the shader
    # dialect from the version string passed to Init) and resolves its entry
    # points through its own loader.
    # IMGUI_IMPL_OPENGL_ES3 makes the backend include <GLES3/gl3.h>, which
    # only exists where an ES dev stack is present (it is the same condition
    # that sets XUO_GLES_DESKTOP in the root CMakeLists). Everywhere else
    # (Windows, macOS, musl) the backend falls back to its bundled loader
    # header; the shader dialect still comes from the runtime version string.
    if (XUO_GLES_DESKTOP)
        target_compile_definitions(${PROJECT_NAME} PRIVATE XUO_GL_RUNTIME=1 XUO_GLES_DESKTOP=1 IMGUI_IMPL_OPENGL_ES3)
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE XUO_GL_RUNTIME=1)
    endif()
endif()
if (USE_GLES2 AND NOT XUO_GLES_DESKTOP AND NOT RENDER_AUTO)
    # The GL3 backend now ships its own loader (imgui_impl_opengl3_loader.h) and
    # selects GLES2 through IMGUI_IMPL_OPENGL_ES2 instead of the old
    # IMGUI_IMPL_OPENGL_LOADER_* macros.
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_ES2)
endif()
# The desktop GLES build runs on an ES3 context and needs the backend's ES3
# mode: gfx.h only describes it per TU, imgui picks it at its own compile time
if (XUO_GLES_DESKTOP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE XUO_GLES_DESKTOP=1 IMGUI_IMPL_OPENGL_ES3)
endif()
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
# Remove warning from 3rd party compilation
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/W0")
else()
    target_compile_options(${PROJECT_NAME} PRIVATE "-w")
endif()
