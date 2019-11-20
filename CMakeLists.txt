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
option(ENABLE_VCPKG "Enable use project-specific vcpkg for managing dependencies (default: On, Windows: always)" ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMake)

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

if (XUO_EMSC)
  set(ENABLE_VCPKG Off)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Build type (Release/Debug/RelWithDebInfo/MinSizeRel)" FORCE)
endif()

if (ENABLE_VCPKG)
  set(AUTO_VCPKG_GIT_TAG "2019.10")
  include(AutoVcpkg)
  vcpkg_install(OpenGL glew SDL2 curl)
  message(STATUS "CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
  include(${CMAKE_TOOLCHAIN_FILE})
endif()

if (XUO_WINDOWS)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
endif()

if (NOT APPLE)
  # Set the preferred opengl library to GLVND if there is GLVND and Legacy.
  # Only if there is no GLVND Legacy will be used.
  # The that is the default with cmake-3.11. Legacy is deprecated.
  # See cmake --help-policy CMP0072
  set(OpenGL_GL_PREFERENCE GLVND)
endif()

find_package(OpenGL REQUIRED)
find_package(GLEW)
set(GL_LIBRARIES OpenGL::GL GLEW::GLEW)

find_package(CURL CONFIG REQUIRED)
set(CURL_LIBRARIES CURL::libcurl)

find_package(SDL2 CONFIG REQUIRED)
get_target_property(SDL2_INCLUDE_DIR SDL2::SDL2-static INTERFACE_INCLUDE_DIRECTORIES)
if (NOT SDL2_INCLUDE_DIR)
  set(SDL2_INCLUDE_DIR "${PACKAGE_PREFIX_DIR}/include/SDL2")
endif ()
set(SDL2_LIBRARIES SDL2::SDL2 SDL2::SDL2main SDL2::SDL2-static)
include_directories("${PACKAGE_PREFIX_DIR}/include/") # catch all include path for dependencies

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
