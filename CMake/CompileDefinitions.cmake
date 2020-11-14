# Add C or C++ compile definitions to the current scope
#
# compile_definitions(def [def ...] [DEBUG_ONLY | RELEASE_ONLY])
#
# Can optionally add the definitions to Debug or Release configurations only, use this so we can
# target multi-configuration generators like Visual Studio or Xcode.
# Release configurations means NOT Debug, so it will work for RelWithDebInfo or MinSizeRel too.
# The definitions are added to the COMPILE_DEFINITIONS folder property.
# Supports generator expressions, unlike add_definitions()
#
# Examples:
#   compile_definitions(FOO) -> -DFOO
#   compile_definitions(_DEBUG DEBUG_ONLY) -> -D_DEBUG
#   compile_definitions(NDEBUG RELEASE_ONLY) -> -DNDEBUG
#   compile_definitions($<$<COMPILE_LANGUAGE:C>:THISISONLYFORC>)
#
# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

function(compile_definitions)
  set(defs ${ARGN})

  list(GET defs -1 last_def)
  list(REMOVE_AT defs -1)

  set(genexp_config_test "1")
  if(last_def STREQUAL "DEBUG_ONLY")
    set(genexp_config_test "$<CONFIG:Debug>")
  elseif(last_def STREQUAL "RELEASE_ONLY")
    set(genexp_config_test "$<NOT:$<CONFIG:Debug>>")
  else()
    list(APPEND defs ${last_def})
  endif()

  set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
    "$<${genexp_config_test}:${defs}>")
endfunction()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DNOMINMAX)
  add_definitions(-DUNICODE)
  add_definitions(-D_UNICODE)
  add_definitions(-DWIN32_LEAN_AND_MEAN)
  add_definitions(-D_WIN32_WINNT=0x0602)
  add_definitions(-D_SCL_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
  add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
  add_definitions(/wd4738) # storing 32-bit float result in memory, possible loss of performance
  add_definitions(/wd4201) # nameless struct/union
else()
  #thread sanitizer - issues to fix
  #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
  #set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
  #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=memory -fsanitize-memory-track-origins -fsanitize-memory-use-after-dtor -fsanitize-blacklist=blacklist.txt")
  #set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=memory -fsanitize-memory-track-origins -fsanitize-memory-use-after-dtor -fsanitize-blacklist=blacklist.txt")
  #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=undefined")
  #set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=undefined")
endif()

# These aren't actually needed for C11/C++11
# but some dependencies require them (LLVM, libav).
add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-D__STDC_CONSTANT_MACROS)
