cmake_minimum_required(VERSION 3.5)
find_program(CMAKE_C_COMPILER NAMES $ENV{CC} clang PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_CXX_COMPILER NAMES $ENV{CXX} clang++ PATHS ENV PATH NO_DEFAULT_PATH)

project(crossuo)

#
# Custom Build Options
#
option(ENABLE_LTO "Enables Link Time Optimization" ON)
option(ENABLE_COTIRE "Disables use of cotire build time reducer" ON)

list(APPEND CMAKE_MODULE_PATH
  ${CMAKE_SOURCE_DIR}/CMake
)

# Enable folders for IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Libraries to link
set(LIBS)

# CMake Customizations
include(cotire)
include(CheckAndAddFlag)
include(CompileDefinitions)
include(Compiler)
include(CheckCCompilerFlag)
include(CCache)
include(ClangFormat)
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

set(EXTERNAL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external")

if(XUO_WINDOWS)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")

	if(NOT "${CMAKE_GENERATOR}" MATCHES "(Win64|IA64)")
		set(XUO_ARCH "x86")
		set(XUO_64BITS 0)
	else()
		set(XUO_ARCH "x64")
		set(XUO_64BITS 1)
	endif()

	set(DEP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Dependencies/include")
	set(DEP_LIB_DIR "${CMAKE_SOURCE_DIR}/Dependencies/${XUO_ARCH}/lib/")

	set(GLEW_INCLUDE_DIR "${DEP_INCLUDE_DIR}")
	set(GLEW_LIBRARIES "${DEP_LIB_DIR}glew32.lib")

	set(SDL2_INCLUDE_DIRS "${DEP_INCLUDE_DIR}/SDL2")
	set(SDL2_LIBRARIES "${DEP_LIB_DIR}sdl2.lib" "${DEP_LIB_DIR}sdl2main.lib")

	# temporary hijacking to use other libs
	set(SDL2_LIBRARIES ${SDL2_LIBRARIES} "${DEP_LIB_DIR}psapi.lib")
else()
	set(XUO_WISP OFF)
	set(XUO_BASS OFF)
	find_package(SDL2 REQUIRED)
	find_package(GLEW REQUIRED)
	# Set the preferred opengl library to GLVND if there is GLVND and Legacy.
	# Only if there is no GLVND Legacy will be used.
	# The that is the default with cmake-3.11. Legacy is deprecated.
	# See cmake --help-policy CMP0072
	set(OpenGL_GL_PREFERENCE GLVND)
endif()

compile_definitions(XUO_DEBUG DEBUG_ONLY)
compile_definitions(XUO_RELEASE RELEASE_ONLY)

find_package(OpenGL REQUIRED)

include_directories(${EXTERNAL_INCLUDE_DIR})
include_directories(${DEP_INCLUDE_DIR})
include_directories(${OPENGL_INCLUDE_DIR})
include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${SDL2_IMAGE_INCLUDE_DIR})
include_directories(${GLEW_INCLUDE_DIR})
#
# Project Files
#

configure_file(
  "${PROJECT_SOURCE_DIR}/src/GitRevision.h.in"
  "${PROJECT_BINARY_DIR}/GitRevision.h"
)
include_directories("${PROJECT_BINARY_DIR}/")

add_subdirectory(src)
