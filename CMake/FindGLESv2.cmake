# - Try to find OpenGLES and EGL
# Once done this will define
#
#  GLESv2_FOUND        - system has OpenGLES
#  GLESv2_INCLUDE_DIR  - the GL include directory
#  GLESv2_LIBRARIES    - Link these to use OpenGLES
#
#  EGL_FOUND        - system has EGL
#  EGL_INCLUDE_DIR  - the EGL include directory
#  EGL_LIBRARIES    - Link these to use EGL

find_package(PkgConfig)
set(_GLESv2_REQUIRED_VARS GLESv2_LIBRARY_DIR GLESv2_INCLUDE_DIR)
set(CMAKE_FIND_LIBRARY_CUSTOM_LIB_SUFFIX "")

# cross toolchains restrict finds to their sysroot (FIND_ROOT_PATH), but the
# dockcross containers carry the GLES2 dev files in the container's own
# multiarch dirs - search those explicitly (NO_CMAKE_FIND_ROOT_PATH)
find_library(GLESv2_LIBRARY_DIR
    NAMES
        GLESv2
    HINTS
        /opt/alpine-gles/usr/lib
        /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/mesa-egl
        /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
        /usr/lib/x86_64-linux-gnu/mesa-egl
        /usr/lib/arm-linux-gnueabihf
        /usr/lib/aarch64-linux-gnu
        /usr/lib/i386-linux-gnu
        /usr/lib
    NO_CMAKE_FIND_ROOT_PATH
)

find_path(GLESv2_INCLUDE_DIR
    NAMES
        gl2.h
    PATHS
        /opt/alpine-gles/usr/include/GLES2
        /usr/include/GLES2
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT ${GLESv2_LIBRARY_DIR}_NOT_FOUND OR GLESv2_FOUND)
    # append result to pkgconfig's results
    set(GLESv2_LIBRARY_DIRS ${GLESv2_LIBRARY_DIRS} ${GLESv2_LIBRARY_DIR})
    message(STATUS "Found libGLESv2.so at ${GLESv2_LIBRARY_DIRS}")
else()
    message(WARNING "no libGLESv2.so found")
endif()


if(NOT ${GLESv2_INCLUDE_DIR}_NOT_FOUND)
    # consumers must include <GLES2/gl2.h>, so the search root's PARENT
    # (e.g. /usr/include) is the usable include dir, not .../GLES2 itself
    set(GLESv2_INCLUDE_DIRS ${GLESv2_INCLUDE_DIRS} ${GLESv2_INCLUDE_DIR})
    get_filename_component(_GLES2_PARENT "${GLESv2_INCLUDE_DIR}/.." ABSOLUTE)
    set(GLES2_INCLUDE_ROOT ${_GLES2_PARENT})
    message(STATUS "Found gl2.h at ${GLESv2_INCLUDE_DIRS}")
else()
    message(WARNING "no gl2.h found in /usr/include/GLES2")
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GLESv2 REQUIRED_VARS ${_GLESv2_REQUIRED_VARS}
	                          HANDLE_COMPONENTS)
unset(_GLESv2_REQUIRED_VARS)

if(NOT GLESv2_FOUND)
    message(STATUS "GLESv2_FOUND NOT found")
else()
    message(STATUS "GLES found")
    message(STATUS "-l: ${GLESv2_LIBRARIES}")
    message(STATUS "-L: ${GLESv2_LIBRARY_DIRS}")
    message(STATUS "-I: ${GLESv2_INCLUDE_DIRS}")
endif()

pkg_search_module(EGL egl)
if(NOT EGL_FOUND)
    # pkg-config may not know about the container's cross GLES/EGL files
    # (restricted PKG_CONFIG_PATH in dockcross) - fall back to plain finds
    find_path(EGL_INCLUDE_DIR
        NAMES EGL/egl.h
        PATHS /usr/include
        NO_CMAKE_FIND_ROOT_PATH)
    find_library(EGL_LIBRARY
        NAMES EGL
        HINTS
            /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
            /usr/lib/arm-linux-gnueabihf
            /usr/lib/aarch64-linux-gnu
            /usr/lib/i386-linux-gnu
            /usr/lib
        NO_CMAKE_FIND_ROOT_PATH)
    if(EGL_INCLUDE_DIR AND EGL_LIBRARY)
        set(EGL_FOUND 1)
        set(EGL_INCLUDE_DIRS ${EGL_INCLUDE_DIR})
        set(EGL_LIBRARIES ${EGL_LIBRARY})
    endif()
endif()
