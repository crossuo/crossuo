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
  add_definitions(/Wall)
  add_definitions(/wd4710) # function not inlined
  add_definitions(/wd4820) # padding added after data member
  add_definitions(/wd4668) # macro is not defined as a preprocessor macro
  add_definitions(/wd5039) # ptr passed to extern C function, UB if exception is thrown
  add_definitions(/wd4626) # assignment op implicitly deleted
  add_definitions(/wd4623) # default ctor implicitly deleted
  add_definitions(/wd5027) # move op implicitly deleted
  add_definitions(/wd5026) # move ctor implicitly deleted
  add_definitions(/wd4625) # copy ctor implicitly deleted
  add_definitions(/wd4365) # signed/unsigned mismatch (TEMPORARY disabled)
  add_definitions(/wd4388) # signed/unsigned mismatch (TEMPORARY disabled)
  add_definitions(/wd4245) # signed/unsigned mismatch (TEMPORARY disabled)
  add_definitions(/wd4389) # signed/unsigned mismatch (TEMPORARY disabled)
  add_definitions(/wd4242) # narrowing conversion (TEMPORARY disabled)
  add_definitions(/wd4244) # narrowing conversion (TEMPORARY disabled)
  add_definitions(/wd4267) # narrowing conversion (TEMPORARY disabled)
  add_definitions(/wd4100) # unused var (TEMPORARY disabled)
  add_definitions(/wd4061) # unhandled case in switch (TEMPORARY disabled)
  add_definitions(/wd4127) # consider using 'if constexpr' (TEMP disabled)
  add_definitions(/wd4191) # unsafe conversion (TEMP disabled)
  add_definitions(/wd4571) # catch semantics changed
  add_definitions(/wd4201) # nameless struct/union
  add_definitions(/wd4265) # no virtual dtor in class with virtual funcs
  add_definitions(/wd4464) # relative include path contains '..'
  add_definitions(/wd4774) # format string expected is not a string literal
  add_definitions(/wd4095) # ???
  add_definitions(/wd4548) # expression before comma has no effect
  add_definitions(/wd4371) # layout of class may have changed from a prev compiler
  add_definitions(/wd4125) # decimal digit terminates octal escape seq
  add_definitions(/wd4505) # unreferenced local function has been removed (TODO disable this on debug only)
  add_definitions(/wd4711) # function selected for automatic inline
  add_definitions(/wd4514) # unreferenced inline func has been removed
  add_definitions(/we4238) # equivalent to gcc/clang -fpermissive
  add_definitions(/wd5045) # Compiler will insert Spectre mitigation for memory load
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
