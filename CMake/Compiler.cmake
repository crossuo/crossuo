# Compiler Detection and Build Specifics
if(CMAKE_GENERATOR MATCHES "Ninja")
  check_and_add_flag(DIAGNOSTICS_COLOR -fdiagnostics-color)
elseif(CMAKE_GENERATOR MATCHES "Visual Studio")
  add_compile_options("/MP")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  compile_definitions(_DEBUG DEBUG_ONLY)
  compile_definitions(_CRT_NONSTDC_NO_DEPRECATE)
  check_and_add_flag(EXCEPTIONS /EHsc)

  # Remove unreferenced inline functions/data to reduce link time and catch bugs
  add_compile_options(/Zc:inline)
  # Assume `new` (w/o std::nothrow) throws to reduce binary size
  add_compile_options(/Zc:throwingNew)
  # Enforce strict volatile semantics as per ISO C++
  add_compile_options(/volatile:iso)

  string(APPEND CMAKE_EXE_LINKER_FLAGS " /NXCOMPAT")
else()
  compile_definitions(_DEBUG DEBUG_ONLY)
  check_and_add_flag(HAVE_WALL -Wall)
  check_and_add_flag(EXTRA -Wextra)
  if ($ENV{TRAVIS_COMPILER})
    # if we are in travis-ci, thread warning as errors
    check_and_add_flag(WARN_AS_ERROR -Werror)
  endif()

  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    check_and_add_flag(NO_STRICT_ALIASING -fno-strict-aliasing)
    check_and_add_flag(NO_TAUTOLOGICAL_COMPARE -Wno-tautological-constant-out-of-range-compare)
    check_and_add_flag(SWITCH_DEFAULT -Wswitch-default)
  elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GCC")
    check_and_add_flag(NO_MISSING_DECL -Wno-missing-declarations) # CommonInterfaces.cpp
    check_and_add_flag(NO_SWITCH_DEFAULT -Wno-switch-default) # SDL_stdinc.h
    check_and_add_flag(NO_TAUTOLOGICAL_COMPARE -Wno-type-limits) # tautological compare
  endif()

  # these are the only we want to really always disable
  check_and_add_flag(NO_SIGN_COMPARE -Wno-sign-compare)
  check_and_add_flag(NO_UNUSED_PARAMETER -Wno-unused-parameter)
  check_and_add_flag(MISSING_FIELD_INITIALIZERS -Wmissing-field-initializers)
  check_and_add_flag(FLOAT_EQUAL -Wfloat-equal)
  #check_and_add_flag(CONVERSION -Wconversion)
  #check_and_add_flag(ZERO_AS_NULL_POINTER_CONSTANT -Wzero-as-null-pointer-constant)

  check_and_add_flag(TYPE_LIMITS -Wtype-limits)
  check_and_add_flag(IGNORED_QUALIFIERS -Wignored-qualifiers)
  check_and_add_flag(UNINITIALIZED -Wuninitialized)
  check_and_add_flag(LOGICAL_OP -Wlogical-op)
  check_and_add_flag(SHADOW -Wshadow)
  check_and_add_flag(INIT_SELF -Winit-self)
  check_and_add_flag(MISSING_DECLARATIONS -Wmissing-declarations)
  check_and_add_flag(MISSING_VARIABLE_DECLARATIONS -Wmissing-variable-declarations)

  check_and_add_flag(VISIBILITY_INLINES_HIDDEN -fvisibility-inlines-hidden)
  check_and_add_flag(VISIBILITY_HIDDEN -fvisibility=hidden)
  check_and_add_flag(PERMISSIVE -fpermissive) # FIXME GCC
  check_and_add_flag(NORTTI -fno-rtti)
  check_and_add_flag(NO_EXCEPTIONS -fno-exceptions)
  check_and_add_flag(FOMIT_FRAME_POINTER -fomit-frame-pointer RELEASE_ONLY)

  check_and_add_flag(GGDB -ggdb DEBUG_ONLY)
endif()

# Compiler extensions

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  # enable the latest C++ standard feature set,
  # and also disable MSVC specific extensions
  # to be even more standards compliant.
  check_and_add_flag(CPPLATEST /std:c++latest)
else()
  # Enable C++17, but fall back to C++14 if it isn't available.
  # CMAKE_CXX_STANDARD cannot be used here because we require C++14 or newer, not any standard.
  #check_and_add_flag(CXX17 -std=c++17)
  if(NOT FLAG_CXX_CXX17)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
  endif()
  check_and_add_flag(THREADS -pthread)
  set(THREAD "-pthread")
  set(LOADER "-ldl")
endif()

if(ENABLE_LTO)
  check_and_add_flag(LTO -flto)
  if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    set(CMAKE_AR gcc-ar)
    set(CMAKE_RANLIB gcc-ranlib)
  endif()
endif()

add_definitions(-DXUO_CMAKE)
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
