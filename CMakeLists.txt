cmake_minimum_required(VERSION 3.12)
find_program(CMAKE_C_COMPILER NAMES $ENV{CC} clang PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_CXX_COMPILER NAMES $ENV{CXX} clang++ PATHS ENV PATH NO_DEFAULT_PATH)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

project(crossuo)

#
# Custom Build Options
#
option(ENABLE_LTO "Enables Link Time Optimization (default: On)" ON)
option(ENABLE_COTIRE "Enables of cotire build time reducer (default: On)" ON)
option(ENABLE_FORMAT "Enables clang-format (default: On)" ON)

list(APPEND CMAKE_MODULE_PATH
  ${CMAKE_SOURCE_DIR}/CMake
)

# Enable folders for IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Libraries to link
set(LIBS)

# CMake Customizations
if (ENABLE_COTIRE)
  include(cotire)
endif()
if (ENABLE_FORMAT)
  include(ClangFormat)
endif()
include(CheckAndAddFlag)
include(CompileDefinitions)
include(Compiler)
include(CheckCCompilerFlag)
include(CCache)
include(WindowsDeps)
include(Revision)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Build type (Release/Debug/RelWithDebInfo/MinSizeRel)" FORCE)
endif()

#
# Dependencies
#

include(CheckCXXSourceRuns)

#
# libcurl 7.50
#
if(NOT XUO_WINDOWS)
    set(CURL_PROJECT curl-7.50.1)
    set(CURL_ARGS ${EXTERNALS_CMAKE_ARGS} -DBUILD_TESTING=Off -DBUILD_CURL_EXE=Off -DBUILD_SHARED_LIBS=Off -DENABLE_ARES=Off -DCURL_STATICLIB=On -DHTTP_ONLY=On)
    ExternalProject_Add(${CURL_PROJECT}
      GIT_REPOSITORY https://github.com/curl/curl.git
      GIT_TAG curl-7_50_1
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/curl
      CMAKE_ARGS 
        -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/curl
        ${CURL_ARGS}
    )
    set(CURL_LIBRARIES curl)
endif()
set(CURL_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/curl/include" CACHE INTERNAL "")


if (NOT APPLE)
  # Set the preferred opengl library to GLVND if there is GLVND and Legacy.
  # Only if there is no GLVND Legacy will be used.
  # The that is the default with cmake-3.11. Legacy is deprecated.
  # See cmake --help-policy CMP0072
  set(OpenGL_GL_PREFERENCE GLVND)
  find_package(OpenGL REQUIRED)
endif()

if(XUO_WINDOWS)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")

  if(NOT "${CMAKE_GENERATOR}" MATCHES "(Win32|IA32|x86)")
    set(XUO_ARCH "x64")
    set(XUO_64BITS 1)
  else()
    set(XUO_ARCH "x86")
    set(XUO_64BITS 0)
  endif()

  set(DEP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/include")
  set(DEP_LIB_DIR "${CMAKE_SOURCE_DIR}/Dependencies/${XUO_ARCH}/lib/")

  set(GL_INCLUDE_DIR "${DEP_INCLUDE_DIR}" ${OPENGL_INCLUDE_DIR})
  set(GL_LIBRARIES "${DEP_LIB_DIR}glew32.lib" ${OPENGL_LIBRARIES})

  set(SDL2_INCLUDE_DIRS "${DEP_INCLUDE_DIR}/SDL2")
  set(SDL2_LIBRARIES "${DEP_LIB_DIR}sdl2.lib" "${DEP_LIB_DIR}sdl2main.lib")

  set(CURL_LIBRARIES "${DEP_LIB_DIR}libcurl.lib")

  # temporary hijacking to use other libs
  set(SDL2_LIBRARIES ${SDL2_LIBRARIES} "${DEP_LIB_DIR}psapi.lib")
else()
  find_package(SDL2 REQUIRED)
  find_package(GLEW REQUIRED)
  set(GL_INCLUDE_DIR ${OPENGL_INCLUDE_DIR})
  if (TARGET GLEW::GLEW)
    set(_GL_TARGETS ${_GL_TARGETS} GLEW::GLEW)
  else()
    set(_GL_TARGETS ${_GL_TARGETS} ${GLEW_LIBRARIES})
  endif()
  if (NOT APPLE)
    if (TARGET OpenGL::OpenGL)
      set(_GL_TARGETS ${_GL_TARGETS} OpenGL::OpenGL "-lGL")
    else()
      set(_GL_TARGETS ${_GL_TARGETS} ${OPENGL_LIBRARIES})
    endif()
  endif()
  set(GL_LIBRARIES ${_GL_TARGETS})
endif()

compile_definitions(XUO_DEBUG DEBUG_ONLY)
compile_definitions(XUO_RELEASE RELEASE_ONLY)

include_directories(${DEP_INCLUDE_DIR})
include_directories(${GL_INCLUDE_DIR})
include_directories(${SDL2_INCLUDE_DIRS})

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
