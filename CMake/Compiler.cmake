
function(add_cxx_option flag)
  if(ARGV1 STREQUAL "RELEASE_ONLY")
    set(should_add "$<CONFIG:Release>")
  else()
    set(should_add 1)
  endif()
  set(is_cxx "$<COMPILE_LANGUAGE:CXX>")
  add_compile_options("$<$<AND:${is_cxx},${should_add}>:${flag}>")
endfunction()

function(add_release_option flag)
  add_compile_options("$<$<CONFIG:Release>:${flag}>")
endfunction()

function(add_debug_option flag)
  add_compile_options("$<$<NOT:$<CONFIG:Release>>:${flag}>")
endfunction()

if(CMAKE_GENERATOR MATCHES "Ninja")
  add_compile_options(-fdiagnostics-color)
elseif(CMAKE_GENERATOR MATCHES "Visual Studio")
  add_compile_options("/MP")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  compile_definitions(_DEBUG DEBUG_ONLY)
  compile_definitions(_CRT_NONSTDC_NO_DEPRECATE)
  add_cxx_option(/EHsc)

  # Remove unreferenced inline functions/data to reduce link time and catch bugs
  add_compile_options(/Zc:inline)
  # Assume `new` (w/o std::nothrow) throws to reduce binary size
  add_cxx_option(/Zc:throwingNew)
  # Enforce strict volatile semantics as per ISO C++
  add_cxx_option(/volatile:iso)

  string(APPEND CMAKE_EXE_LINKER_FLAGS " /NXCOMPAT")
else()
  compile_definitions(_DEBUG DEBUG_ONLY)
  add_compile_options(-Wall)
  add_compile_options(-Wextra)
  if ($ENV{TRAVIS_COMPILER})
    # if we are in travis-ci, thread warning as errors
    add_compile_options(-Werror)
  endif()

  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fno-strict-aliasing)
    add_compile_options(-Wno-tautological-constant-out-of-range-compare)
    add_compile_options(-Wswitch-default)
    add_compile_options(-Wmissing-variable-declarations)
    add_compile_options(-Wmissing-declarations)
  elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GCC")
    add_compile_options(-Wno-missing-declarations) # CommonInterfaces.cpp
    add_compile_options(-Wno-switch-default) # SDL_stdinc.h
    add_compile_options(-Wno-type-limits) # tautological compare
    add_compile_options(-Wlogical-op)
  endif()

  # these are the only we want to really always disable
  add_compile_options(-Wno-sign-compare)
  add_compile_options(-Wno-unused-parameter)
  add_compile_options(-Wmissing-field-initializers)
  add_compile_options(-Wfloat-equal)
  #add_compile_options(-Wconversion)
  #add_compile_options(-Wzero-as-null-pointer-constant)

  add_compile_options(-Wtype-limits)
  add_compile_options(-Wignored-qualifiers)
  add_compile_options(-Wuninitialized)
  add_compile_options(-Wshadow)
  add_compile_options(-Winit-self)

  #add_cxx_option(-fvisibility-inlines-hidden)
  #add_cxx_option(-fvisibility=hidden)
  #add_cxx_option(-fpermissive) # FIXME GCC
  add_cxx_option(-fno-rtti)
  add_cxx_option(-fno-exceptions)
  add_cxx_option(-fomit-frame-pointer RELEASE_ONLY)
  add_debug_option(-g)

  add_compile_options(-Wno-unknown-pragmas)
endif()

# Compiler extensions

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  # enable the latest C++ standard feature set,
  # and also disable MSVC specific extensions
  # to be even more standards compliant.
  add_cxx_option(/std:c++latest)
else()
  #add_compile_options(-pthread)
  set(THREAD "-pthread")
  set(LOADER "-ldl")
endif()

if(ENABLE_LTO)
  #add_compile_options(-flto)
  #if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  #  set(CMAKE_AR gcc-ar)
  #  set(CMAKE_RANLIB gcc-ranlib)
  #endif()
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DXUO_WINDOWS=1)
  set(XUO_WINDOWS 1)
elseif(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
  add_definitions(-DXUO_EMSC=1)
  set(XUO_EMSC 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_definitions(-DXUO_LINUX=1)
  set(XUO_LINUX 1)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  add_definitions(-DXUO_OSX=1)
  set(XUO_OSX 1)
endif()
