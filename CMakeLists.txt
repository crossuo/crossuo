# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

cmake_minimum_required(VERSION 3.13)
# Vendored deps declare cmake_minimum_required older than 3.5, which CMake >= 4
# rejects; allow configuring them anyway.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
find_program(CMAKE_C_COMPILER NAMES $ENV{CC} clang PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_CXX_COMPILER NAMES $ENV{CXX} clang++ PATHS ENV PATH NO_DEFAULT_PATH)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

project(crossuo)

if (XUO_WINDOWS AND NOT MSVC)
  # mingw cross builds: .rc resources need the RC (windres) language enabled;
  # MSVC generators enable it automatically
  enable_language(RC)
  # windres resolves ICON/BITMAP files against its include paths, not the .rc
  # file's directory like MSVC's rc.exe does
  set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I${PROJECT_SOURCE_DIR}/resources")
endif()

if (NOT XUO_VERSION)
  set(XUO_VERSION "2.0.0")
endif()

string(REPLACE "." "," XUO_VERSION_NUM ${XUO_VERSION})
set(XUO_VERSION_NUM "0,${XUO_VERSION_NUM}")
message(STATUS "Version: ${XUO_VERSION} (${XUO_VERSION_NUM})")

#
# Custom Build Options
#
option(ENABLE_LTO "Enables Link Time Optimization (default: On)" ON)
option(ENABLE_FORMAT "Enables clang-format (default: On)" ON)
option(RENDER_GLES2 "Force OpenGL ES2 (default Off; web builds only, unsupported on desktop)" OFF)
option(RENDER_GL3 "Force OpenGL 3.x (default Off)" OFF)
option(RENDER_GL1 "Force OpenGL 1.1 (automatic)" OFF)
option(XUO_ASSISTANT "Build in-client assistant (default: On)" ON)
option(XUO_STATIC_BINARY "Fully static link the client executable (musl) (default: Off)" OFF)
option(XUO_STATIC_CXX_RUNTIME "Statically link libgcc/libstdc++ (portable glibc builds) (default: Off)" OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)

# Enable folders for IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# CMake Customizations
if (ENABLE_FORMAT)
  include(ClangFormat)
endif()
include(CompileDefinitions)
include(Compiler)

if (XUO_WINDOWS AND NOT MSVC)
  # winsock must appear after every target archive on the link line (vendored
  # libressl/curl reference it); CMake emits standard libraries last
  set(CMAKE_CXX_STANDARD_LIBRARIES "-lws2_32 ${CMAKE_CXX_STANDARD_LIBRARIES}")
endif()
include(CCache)
include(Revision)
include(Dependencies)
include(Embedding)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Build type (Release/Debug/RelWithDebInfo/MinSizeRel)" FORCE)
endif()

# ImGui debug tools (the id-conflict error checker, the "developer mode"
# assert/error window, the demo window) are a developer aid: end users must
# never see them. Global (not per-target): the macros change inline code in
# imgui's headers, so every TU including imgui.h must agree (ODR).
if (CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo|MinSizeRel")
  add_compile_definitions(IMGUI_DISABLE_DEBUG_TOOLS IMGUI_DISABLE_DEMO_WINDOWS)
endif()

if (NOT RENDER_GLES2 AND NOT RENDER_GL1 AND NOT RENDER_GL3)
  if (XUO_EMSC)
    # the browser only offers WebGL: GLES2 renderer
    set(RENDER_GLES2 On)
  else()
    message(STATUS "No render selected, defaulting to OpenGL 3")
    set(RENDER_GL3 On)
  endif()
endif()

#
# Build Dependencies
#
if (RENDER_GLES2 AND NOT XUO_EMSC)
  if (NOT CMAKE_CROSSCOMPILING)
    # the renderer calls GLES3 entry points (glTexImage3D, glCopyImageSubData,
    # GL_TEXTURE_BINDING_2D_ARRAY, GL_MAX_ARRAY_TEXTURE_LAYERS) that desktop
    # libGLESv2 does not expose, so this combination can never compile
    message(FATAL_ERROR
        "RENDER_GLES2 is not supported on desktop builds: the renderer requires "
        "GLES3 entry points that desktop libGLESv2 lacks.\n"
        "Build the web target for GLES2 (emscripten/WebGL2), or use GL3 "
        "(default) or GL1 (-DRENDER_GL1=On) on desktop.")
  endif()
  # cross builds are toolchain-based: their toolchain files set
  # CMAKE_SYSTEM_NAME, which is what makes CMake set CMAKE_CROSSCOMPILING, so
  # the gate above never fires for e.g. dockcross armv7/musl-arm64
  find_package(GLESv2 REQUIRED)
endif()
if ((RENDER_GL1 OR RENDER_GL3) AND NOT XUO_EMSC)
  if (NOT APPLE)
    if (XUO_STATIC_CXX_RUNTIME)
      # portable glibc builds: prefer legacy libGL (mesa-style, universal) over
      # GLVND, whose libOpenGL.so.0 is missing on older target systems
      set(OpenGL_GL_PREFERENCE LEGACY)
    else()
      # Set the preferred opengl library to GLVND if there is GLVND and Legacy.
      # Only if there is no GLVND Legacy will be used.
      # The that is the default with cmake-3.11. Legacy is deprecated.
      # See cmake --help-policy CMP0072
      set(OpenGL_GL_PREFERENCE GLVND)
    endif()
  endif()
  find_package(OpenGL REQUIRED)
endif()

if (XUO_EMSC)
  # emscripten: GLES2/WebGL1 comes from the browser, SDL2 from the emsdk
  # port (-s USE_SDL=2); nothing GL-related needs finding or linking
  compile_definitions(USE_GLES2=1)
  compile_definitions(USE_GLES)
  set(USE_GLES2 1)
  set(GL_LIBRARIES "")
  message(STATUS "Building with GLES2 renderer (WebGL via emscripten)")
elseif (OPENGL_FOUND)
  if (RENDERER_GL1 OR RENDER_GL1) # RENDERER_GL1 for compatibility, RENDER_GL1 is original
    # RENDER_GL1 selects the GL1 legacy backend (the old GL2 mode is no
    # longer reachable)
    compile_definitions(USE_GL1=1)
    set(USE_GL1 1)
    message(STATUS "Building with GL1 legacy backend")
  else()
    # Modern mode: GL3
    compile_definitions(USE_GL3=1)
    set(USE_GL3 1)
    if (NOT XUO_EMSC)
      # GL3 desktop builds embed the X:UO Launcher inside the client; other
      # renderer builds run without a launcher (configuration via crossuo.cfg
      # / --config).
      # (the -D define itself is attached to the crossuo target only, so the
      # dependency tree is not polluted and rebuilt)
      set(XUO_LAUNCHER_EMBEDDED 1)
      message(STATUS "X:UO Launcher: embedded in crossuo")
    endif()
    message(STATUS "Building with GL3 renderer")
  endif()
  set(GL_LIBRARIES ${OPENGL_LIBRARIES})
  if (CMAKE_SYSROOT AND UNIX AND NOT APPLE)
    # the cross sysroot's libGL.so references libXext.so.6, which some
    # sysroots only ship a few levels deep: add the sysroot lib dirs to the
    # link search path so the static linker resolves the dependency without
    # warnings (rpath-link is not consulted for sysrooted shared libs by
    # some binutils versions, but -L is)
    foreach(_gl_dir "${CMAKE_SYSROOT}/usr/lib" "${CMAKE_SYSROOT}/usr/lib64")
      if (IS_DIRECTORY "${_gl_dir}")
        list(APPEND GL_LIBRARIES "${_gl_dir}")
      endif()
    endforeach()
  endif()
  if (XUO_STATIC_BINARY AND NOT MSVC)
    # fully static client: Mesa has no static archive, so restore dynamic
    # resolution from the GL stack onward (flipped back to static at the end of
    # the crossuo link line, see src/CMakeLists.txt)
    set(GL_LIBRARIES "-Wl,-Bdynamic" ${GL_LIBRARIES})
  endif()
elseif (GLESv2_FOUND)
  compile_definitions(USE_GLES2=1)
  # the src renderer gates key off the generic family flag (USE_GLES - the
  # same name gfx.h derives from USE_GLES2 for the gfx lib), and they sit on
  # the first lines of the TUs (RenderDraw.cpp:4, RenderState.cpp:4,
  # RenderDebug.cpp:4, RenderInternal.cpp:5, ...) BEFORE gfx.h gets included,
  # so the gfx.h mapping never reaches them: define it for the whole tree.
  # Passed without a value to match gfx.h's empty '#define USE_GLES' (a -1
  # value would trigger a macro redefinition warning in every gfx.h user).
  compile_definitions(USE_GLES)
  # the checked-in SDL_config.h enables immintrin.h unconditionally; on GLES2
  # (ARM) builds the x86 intrinsics header is not available - SDL_cpuinfo.h
  # handles its absence via this disable flag
  compile_definitions(SDL_DISABLE_IMMINTRIN_H=1)
  set(USE_GLES2 1)
  # libglvnd's libGLESv2 is a stub over libGLdispatch: both must be linked.
  # musl-built GLES2 (the alpine sysroot) is standalone: it needs the alpine
  # mesa-glapi instead, and no GLdispatch - only search GLdispatch when the
  # GLESv2 came from the (glibc) container prefixes
  if(GLESv2_LIBRARY_DIR MATCHES "^/opt/alpine-gles")
    set(GLdispatch_LIBRARY "")
    find_library(glapi_LIBRARY
        NAMES glapi
        HINTS /opt/alpine-gles/usr/lib
        NO_CMAKE_FIND_ROOT_PATH)
    set(GL_LIBRARIES ${GLESv2_LIBRARY_DIR} ${glapi_LIBRARY})
    if(XUO_STATIC_BINARY)
        list(APPEND GL_LIBRARIES
            "-Wl,-rpath-link,/opt/alpine-gles/usr/lib"
            "-Wl,-rpath-link,/opt/alpine-gles/lib")
    endif()
  else()
    find_library(GLdispatch_LIBRARY
        NAMES GLdispatch
        HINTS
            /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
            /usr/lib/arm-linux-gnueabihf
            /usr/lib/aarch64-linux-gnu
            /usr/lib/i386-linux-gnu
            /usr/lib
        NO_CMAKE_FIND_ROOT_PATH)
    set(GL_LIBRARIES ${GLESv2_LIBRARY_DIR} ${GLdispatch_LIBRARY})
  endif()
  if (XUO_STATIC_BINARY AND NOT MSVC)
    # fully static client: GLES2 has no static archive, so restore dynamic
    # resolution from the GL stack onward (same as the GL3 path above)
    set(GL_LIBRARIES "-Wl,-Bdynamic" ${GL_LIBRARIES})
  endif()
  message(STATUS "Building with GLES2 renderer")
endif()

#
# In-client assistant (X:UO Assist)
#
# the assistant renders through the vendored ImGui GL3/GLES2 stack, so it is
# only available on those renderers (same policy as XUO_LAUNCHER_EMBEDDED
# above); GL1/GL2 legacy builds must not get it
set(XUO_ASSISTANT_ENABLED Off)
if (XUO_ASSISTANT AND (USE_GL3 OR USE_GLES2))
  set(XUO_ASSISTANT_ENABLED On)
  message(STATUS "X:UO Assist: embedded in crossuo")
endif()

set(XUO_LOCAL_HEADERS 1)
# Disable Wayland as it causes multiple definitions errors during linking
set(WAYLAND_SHARED OFF CACHE BOOL "Force disable Wayland")
xuo_dependency(glm glm GLM)
if (NOT XUO_EMSC)
  xuo_dependency(cmcurl cmcurl CURL)
  if (XUO_LAUNCHER_EMBEDDED)
    # the embedded launcher drags the http/updater stack into the crossuo
    # client executable, which requires position independent code in the
    # vendored static curl (libressl already builds with PIC)
    set_property(TARGET cmcurl PROPERTY POSITION_INDEPENDENT_CODE ON)
  endif()
  xuo_dependency(XSDL2 sdl-2.0.10-c1be3add38 SDL2)
  set(3RDPARTY_STATIC SDL2-static cmcurl)
else()
  add_compile_definitions(USE_SDL=2)
  # -pthread: the vfs fetch engine/mft use std::thread and 64-bit atomics,
  # which require the atomics feature (SharedArrayBuffer at runtime - serve
  # with COOP/COEP headers). pthread builds cannot grow memory, so size it
  # upfront.
  # PROXY_TO_PTHREAD: main() runs on a worker so the browser main thread
  # stays free - required for blocking pthread ops (joins/futex/fetch
  # proxying) during the streaming bootstrap; GL renders through an
  # OffscreenCanvas transferred to that worker
  # compile-side settings only: MAX_WEBGL_VERSION/PROXY_TO_PTHREAD/
  # OFFSCREEN_FRAMEBUFFER are linker settings (em++ warns they are ignored
  # during compilation, so they live only on the link line below)
  add_compile_options(-sUSE_SDL=2 -pthread)
  add_link_options(-sUSE_SDL=2 -sMAX_WEBGL_VERSION=2 -pthread -sPROXY_TO_PTHREAD -sOFFSCREEN_FRAMEBUFFER=1 -lidbfs.js -lwebsocket.js)
  # the client comes from desktop-sized stacks: give every thread room
  # (default 64KB overflows before main); pthread builds cannot grow memory,
  # so size the fixed heap for the client + streaming buffers upfront
  add_link_options(-sSTACK_SIZE=8388608 -sINITIAL_MEMORY=1073741824)
  add_link_options(-sPTHREAD_POOL_SIZE=8 -sEXIT_RUNTIME=0)
  add_link_options(-sERROR_ON_UNDEFINED_SYMBOLS=0)
  add_link_options(--profiling-funcs)
  # honor the shell-provided Module fields (arguments/canvas/print/...)
  add_link_options(-sINCOMING_MODULE_JS_API=arguments,canvas,print,printErr,setStatus,onRuntimeInitialized,locateFile)
  # the page shell polls the live download accounting (shell.html ->
  # http.h). The zero-arg i64 getters are NOT exported: in PROXY_TO_PTHREAD
  # builds the exports bind on the worker where the module instantiates, so
  # page-side calls into them resolve to nothing. Instead the shell reads
  # the counters straight out of the shared wasm heap - one tiny u32 export
  # hands over the static pair's address (contiguous, 8-aligned) and HEAPU64
  # does the rest; no cross-thread calls, no shared-heap mallocs. HEAPU64
  # must be exported onto Module: without it the views live only in the glue
  # closure (updateMemoryViews runs on the worker; the page-side module copy
  # never assigns them, so the page has no TypedArray to read through)
  add_link_options(-sEXPORTED_RUNTIME_METHODS=HEAPU64)
  # _xuo_script_pick_addrs: the assistant's file picker bridge (the page
  # reads the status/paths pair out of the shared heap, resources/shell.html)
  add_link_options(-sEXPORTED_FUNCTIONS=_main,_vfs_web_progress_addrs,_xuo_script_pick_addrs)
  # web builds stream from their own origin by default (the browser blocks
  # cross-origin fetches without CORS headers); override per build with
  # -DXUO_WEB_PRODUCT_URL=<url>
  if(NOT XUO_WEB_PRODUCT_URL)
    # relative: resolved against the page origin, so the product must be
    # served next to the wasm (scripts/serve-wasm.py proxies the streaming
    # hosts and serves the local tree at /patch/)
    set(XUO_WEB_PRODUCT_URL "patch/manifest/uo-legacyrelease.prod")
  endif()
  add_compile_definitions(XUO_WEB_PRODUCT_URL="${XUO_WEB_PRODUCT_URL}")
endif()

if (XUO_WINDOWS AND MSVC)
  add_link_options(/ignore:4217 /ignore:4286 /ignore:4049)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
endif()

# DEBUG internal cmake state
#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

compile_definitions(XUO_DEBUG DEBUG_ONLY)
compile_definitions(XUO_RELEASE RELEASE_ONLY)
if (XUO_DEPLOY)
  compile_definitions(XUO_DEPLOY RELEASE_ONLY)
endif()

#
# Project Files
#

create_resource(${PROJECT_SOURCE_DIR}/resources/cover.png ${PROJECT_BINARY_DIR}/cover.h)

configure_file(
  "${PROJECT_SOURCE_DIR}/resources/revision.h.in"
  "${PROJECT_BINARY_DIR}/revision.h"
)
configure_file(
  "${PROJECT_SOURCE_DIR}/resources/icon_launcher.h"
  "${PROJECT_BINARY_DIR}/icon_launcher.h"
  COPYONLY
)
configure_file(
  "${PROJECT_SOURCE_DIR}/resources/icon_client.h"
  "${PROJECT_BINARY_DIR}/icon_client.h"
  COPYONLY
)
include_directories("${PROJECT_BINARY_DIR}/")

if (GLESv2_FOUND)
  # the cross containers may not ship GLES2/EGL headers in their sysroot;
  # the container's own ones (found by CMake/FindGLESv2.cmake) then apply.
  # include_directories(/usr/include) is dropped as an implicit sysroot
  # include, so force it with -idirafter instead
  add_compile_options(-idirafter "${GLES2_INCLUDE_ROOT}")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(armv[0-9]|arm)$")
  # 32-bit ARM has no native 64-bit atomics (quickjs, vfs_trap): route the
  # compiler runtime through libatomic for every target defined below
  link_libraries(atomic)
endif()

add_subdirectory(external)
add_subdirectory(common)
add_subdirectory(xuocore)
add_subdirectory(external/gfx)
if (NOT XUO_EMSC)
  add_subdirectory(tools)
  add_subdirectory(tests)
endif()
add_subdirectory(src)
