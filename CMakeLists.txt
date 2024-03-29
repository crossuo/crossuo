# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

cmake_minimum_required(VERSION 3.13)
find_program(CMAKE_C_COMPILER NAMES $ENV{CC} clang PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_CXX_COMPILER NAMES $ENV{CXX} clang++ PATHS ENV PATH NO_DEFAULT_PATH)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

project(crossuo)

if (NOT XUO_VERSION)
  set(XUO_VERSION "1.0.7")
endif()

string(REPLACE "." "," XUO_VERSION_NUM ${XUO_VERSION})
set(XUO_VERSION_NUM "0,${XUO_VERSION_NUM}")
message(STATUS "Version: ${XUO_VERSION} (${XUO_VERSION_NUM})")

#
# Custom Build Options
#
option(ENABLE_LTO "Enables Link Time Optimization (default: On)" ON)
option(ENABLE_FORMAT "Enables clang-format (default: On)" ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)

# Enable folders for IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# CMake Customizations
if (ENABLE_FORMAT)
  include(ClangFormat)
endif()
include(CompileDefinitions)
include(Compiler)
include(CCache)
include(Revision)
include(Dependencies)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Build type (Release/Debug/RelWithDebInfo/MinSizeRel)" FORCE)
endif()

#
# Build Dependencies
#
if (NOT APPLE)
  # Set the preferred opengl library to GLVND if there is GLVND and Legacy.
  # Only if there is no GLVND Legacy will be used.
  # The that is the default with cmake-3.11. Legacy is deprecated.
  # See cmake --help-policy CMP0072
  set(OpenGL_GL_PREFERENCE GLVND)
endif()

find_package(OpenGL REQUIRED)
set(XUO_LOCAL_HEADERS 1)
# Disable Wayland as it causes multiple definitions errors during linking
set(WAYLAND_SHARED OFF CACHE BOOL "Force disable Wayland")
if (NOT XUO_EMSC)
  xuo_dependency(cmcurl cmcurl CURL)
  xuo_dependency(XSDL2 sdl-2.0.10-c1be3add38 SDL2)
  set(3RDPARTY_STATIC SDL2-static cmcurl)
else()
  add_compile_definitions(USE_SDL=2)
  add_compile_options(-s USE_SDL=2)
endif()

if (XUO_WINDOWS)
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

add_subdirectory(external)
add_subdirectory(common)
add_subdirectory(xuocore)
if (NOT XUO_EMSC)
  add_subdirectory(external/gfx)
  add_subdirectory(tools)
endif()
add_subdirectory(src)
