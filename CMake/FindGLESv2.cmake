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

find_library(GLESv2_LIBRARY_DIR
    NAMES
        GLESv2
    HINTS
        /usr/lib/x86_64-linux-gnu/mesa-egl
        /usr/lib
)

find_path(GLESv2_INCLUDE_DIR
    NAMES
        gl2.h
    PATHS
        /usr/include/GLES2
)

if(NOT ${GLESv2_LIBRARY_DIR}_NOT_FOUND OR GLESv2_FOUND)
    # append result to pkgconfig's results
    set(GLESv2_LIBRARY_DIRS ${GLESv2_LIBRARY_DIRS} ${GLESv2_LIBRARY_DIR})
    message(STATUS "Found libGLESv2.so at ${GLESv2_LIBRARY_DIRS}")
else()
    message(WARNING "no libGLESv2.so found")
endif()


if(NOT ${GLESv2_INCLUDE_DIR}_NOT_FOUND)
    # append result to what pkgconfig's results
    set(GLESv2_INCLUDE_DIRS ${GLESv2_INCLUDE_DIRS} ${GLESv2_INCLUDE_DIR})
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

pkg_search_module(EGL REQUIRED egl)

#if(NOT EGL_FOUND)
#    message(FATAL_ERROR "EGL_FOUND NOT found")
#else()
#    message(STATUS "EGL found")
#    message(STATUS "-l: ${EGL_LIBRARIES}")
#    message(STATUS "-L: ${EGL_LIBRARY_DIRS}")
#    message(STATUS "-I: ${EGL_INCLUDE_DIRS}")
#endif()
