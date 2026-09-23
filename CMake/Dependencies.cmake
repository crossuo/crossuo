# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

macro(xuo_disable_console TARGET)
  if (XUO_WINDOWS)
    if (MSVC)
      set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
    elseif (MINGW)
      # hide the console; the mingw crt (crtexewin.o) adapts WinMain -> main()
      target_link_options(${TARGET} PRIVATE -mwindows)
    endif()
  endif()
endmacro()

macro(xuo_dependency DEP_NAME DEP_PATH DEP_PREFIX)
  message(STATUS "Configuring dependency: ${DEP_NAME}")

  if("${DEP_PREFIX}" MATCHES "CURL")
    if (XUO_WINDOWS)
      set(REQUIRED_LIBS "${REQUIRED_LIBS}" wsock32 crypt32 ws2_32)
    endif()
    if (XUO_LINUX OR XUO_WINDOWS OR XUO_OSX)
      include_directories(BEFORE SYSTEM "${CMAKE_SOURCE_DIR}/deps/cmcurl/lib/libressl/include")
      set(REQUIRED_LIBS "${REQUIRED_LIBS}" "${CMAKE_BINARY_DIR}/deps/cmcurl/lib/libressl/ssl/libssl.a" "${CMAKE_BINARY_DIR}/deps/cmcurl/lib/libressl/crypto/libcrypto.a")
    endif()
    # curl is linked into the client (and the PIC-built xuoassist), so the
    # static lib must be position independent
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  endif()

  if("${DEP_PREFIX}" MATCHES "SDL2")
    if (MINGW)
      # mingw link convention: the crt helper lib precedes SDL2main so the
      # WinMain/SDL_main startup objects resolve in order
      set(REQUIRED_LIBS "${REQUIRED_LIBS}" -lmingw32)
    endif()
    set(REQUIRED_LIBS "${REQUIRED_LIBS}" XSDL2main "${GL_LIBRARIES}")
    add_definitions("-DGLEW_STATIC")
  endif()

  if("${DEP_PREFIX}" MATCHES "glm")
    set(REQUIRED_LIBS "${REQUIRED_LIBS}" glm::glm)
  endif()

  add_subdirectory(${CMAKE_SOURCE_DIR}/deps/${DEP_PATH})

  set(import_file "${CMAKE_BINARY_DIR}/${DEP_PREFIX}.cmake")
  if(EXISTS "${import_file}")
    include("${import_file}")
  endif()
  set(REQUIRED_LIBS "${REQUIRED_LIBS}" "${${DEP_PREFIX}_EXTRA_LIBS}")

  if("${DEP_PREFIX}" MATCHES "glm")
    set(${DEP_PREFIX}_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/deps/${DEP_PATH}" CACHE INTERNAL "deps" FORCE)
    set(${DEP_PREFIX}_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/deps/${DEP_PATH}" CACHE INTERNAL "deps" FORCE)
  else()
    set(${DEP_PREFIX}_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/deps/${DEP_PATH}/include" CACHE INTERNAL "deps" FORCE)
    set(${DEP_PREFIX}_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/deps/${DEP_PATH}/include" CACHE INTERNAL "deps" FORCE)
  endif()
  set(${DEP_PREFIX}_LIBRARY "${DEP_NAME}" CACHE INTERNAL "deps" FORCE)
  set(${DEP_PREFIX}_LIBRARIES "${${DEP_PREFIX}_LIBRARY}" "${REQUIRED_LIBS}" CACHE INTERNAL "deps" FORCE)
  if (XUO_WINDOWS)
    set(${DEP_PREFIX}_LIBRARY_DIRS
      "${${DEP_PREFIX}_LIBRARY_DIRS}"
      "${CMAKE_BINARY_DIR}/deps/${DEP_PATH}"
      "${CMAKE_BINARY_DIR}/deps/${DEP_PATH}/lib"
      "${CMAKE_BINARY_DIR}/deps/${DEP_PATH}/${CMAKE_BUILD_TYPE_INIT}"
      "${CMAKE_BINARY_DIR}/deps/${DEP_PATH}/lib/${CMAKE_BUILD_TYPE_INIT}"
      CACHE INTERNAL "deps" FORCE)
  else()
    set(${DEP_PREFIX}_LIBRARY_DIRS
      "${CMAKE_BINARY_DIR}/deps/${DEP_PATH}"
      "${CMAKE_BINARY_DIR}/deps/${DEP_PATH}/lib"
      CACHE INTERNAL "deps" FORCE)
  endif()

  include_directories(BEFORE SYSTEM ${${DEP_PREFIX}_INCLUDE_DIRS})
  link_directories(${${DEP_PREFIX}_LIBRARY_DIRS})

  add_definitions("-D${DEP_PREFIX}_STATICLIB")
endmacro()
