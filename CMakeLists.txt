cmake_minimum_required(VERSION 3.12)
find_program(CMAKE_C_COMPILER NAMES $ENV{CC} clang PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_CXX_COMPILER NAMES $ENV{CXX} clang++ PATHS ENV PATH NO_DEFAULT_PATH)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

project(crossuo)

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
xuo_dependency(cmcurl cmcurl CURL)
xuo_dependency(XSDL2 sdl-2.0.10-c1be3add38 SDL2)

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

#
# Project Files
#

configure_file(
  "${PROJECT_SOURCE_DIR}/src/GitRevision.h.in"
  "${PROJECT_BINARY_DIR}/GitRevision.h"
)
include_directories("${PROJECT_BINARY_DIR}/")

add_subdirectory(external)
add_subdirectory(external/gfx)
add_subdirectory(common)
add_subdirectory(xuocore)
add_subdirectory(tools)
add_subdirectory(src)
