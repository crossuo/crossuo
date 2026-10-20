# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

# X:UO Assist: in-client ImGui assistant, linked into the crossuo target
# (wired by src/CMakeLists.txt, guarded by the root CMakeLists
# XUO_ASSISTANT / XUO_ASSISTANT_ENABLED)

set(XUOASSIST_PROJECT "xuoassist" CACHE INTERNAL "")
set(XUOASSIST_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "")

set(XUOASSIST_SRCS
  "AssistantWindow.cpp"
  "ui/assistantui.cpp"
  "ui/assistanteventsui.cpp"
  "ui/widgets.cpp"
  "ui/tabmain.cpp"
  "ui/tablists.cpp"
  "ui/tabfilters.cpp"
  "core/hotkeys.cpp"
  "core/actions.cpp"
  "macros/macroexecutor.cpp"
  "ui/hotkeysform.cpp"
  "ui/tabhotkeys.cpp"
  "ui/tabmacros.cpp"
  "ui/macroscriptgen.cpp"
  "core/dressagent.cpp"
  "core/shopagent.cpp"
  "ui/tabdress.cpp"
  "ui/tabparty.cpp"
  "ui/tabshop.cpp"
  "ui/tabskills.cpp"
  "ui/tabdisplay.cpp"
  "script/commandqueue.cpp"
  "script/journal.cpp"
  "script/engine.cpp"
  "script/apiscript.cpp"
  "script/scriptfile.cpp"
  "script/scriptlist.cpp"
  "script/scriptgump.cpp"
  "script/scriptstate.cpp"
  "script/scripthandle.cpp"
  "ui/scriptmenu.cpp"
  "ui/editor.cpp"
  "ui/scripteditor.cpp"
  "ui/editors.cpp"
  "ui/scriptpicker.cpp"
  "ui/tabscripts.cpp"
  "managers/assmap.cpp"
  "managers/mapsettings.cpp"
  "ui/mapwindow.cpp"
  "ui/mapsettings.cpp"
  "ui/inspector.cpp"
  "ui/tababout.cpp"
  "ui/textdialog.cpp"
  "core/global.cpp"
  "core/worldutil.cpp"
  "core/textconv.cpp"
  "core/assistantevents.cpp"
  "core/assist.cpp"
  "core/textcommandmanager.cpp"
  "managers/contextmenumanager.cpp"
  "managers/displaymanager.cpp"
  "managers/gumpmanager.cpp"
  "managers/guildmanager.cpp"
  "managers/menumanager.cpp"
  "managers/promptmanager.cpp"
  "managers/spellmanager.cpp"
  "managers/trademanager.cpp"
  "macros/macro.cpp"
  "config/configio.cpp"
  "config/assistantconfig.cpp"
  "model/commonitems/finditem.cpp"
  "model/commonitems/ignoreitem.cpp"
  "model/commonitems/gumphook.cpp"
  "model/commonitems/gumpitem.cpp"
  "model/commonitems/journalmessage.cpp"
  "model/commonitems/menu.cpp"
  "model/commonitems/menuitem.cpp"
  "model/commonitems/prompt.cpp"
  "model/commonitems/shopitem.cpp"
  "model/commonitems/tradewindow.cpp"
  "model/commonitems/vendorshopitem.cpp"
)

add_library(xuoassist STATIC ${XUOASSIST_SRCS})

# Vendored QuickJS-ng (script/quickjs/VENDOR.md): the four upstream C sources
# compile straight into the lib; no upstream file is modified.
target_sources(xuoassist PRIVATE
  script/quickjs/dtoa.c
  script/quickjs/libregexp.c
  script/quickjs/libunicode.c
  script/quickjs/quickjs.c
)
set_source_files_properties(
  script/quickjs/dtoa.c
  script/quickjs/libregexp.c
  script/quickjs/libunicode.c
  script/quickjs/quickjs.c
  PROPERTIES COMPILE_OPTIONS "-w"
)
# the define lives on this target for its own sources and on crossuo for the
# hook call sites
target_compile_definitions(xuoassist PRIVATE XUO_ASSISTANT=1)
# linked into the single self-contained client executable, so it needs
# position independent code
set_property(TARGET xuoassist PROPERTY POSITION_INDEPENDENT_CODE ON)

add_dependencies(xuoassist ${EXTERNALGFX_PROJECT} ${XUOCORE_PROJECT} ${COMMON_PROJECT})
target_link_libraries(xuoassist PUBLIC ${EXTERNALGFX_PROJECT} ${XUOCORE_PROJECT} ${COMMON_PROJECT})
target_include_directories(xuoassist PUBLIC
  ${XUOASSIST_INCLUDE}
  ${EXTERNALGFX_INCLUDE}
  ${COMMON_INCLUDE}
  ${CMAKE_SOURCE_DIR}/src
)

# # the script-runtime harness (no client deps; the client-facing
# symbols and the engine flow are stubbed in the harness main)
option(XUO_ASSISTANT_SCRIPT_TESTS "Build the xuoassist script harness" OFF)
if(XUO_ASSISTANT_SCRIPT_TESTS)
  add_executable(xuoassist-script-test
    script/test/harness.cpp
    script/test/harnessstubs.cpp
    # the REAL registration + drain executor (the class-wiring assertions run
    # against it; the client-facing symbols are stubbed in harnessstubs.cpp)
    script/apiscript.cpp
    script/commandqueue.cpp
    script/journal.cpp
    script/scriptfile.cpp
    script/scriptlist.cpp
    script/scriptgump.cpp
    script/scriptstate.cpp
    # the config module (the INI reader/writer + the crash-safe save helper;
    # ScriptTextWindow_SaveToFile calls AtomicFileWrite and the round-trip
    # tests exercise CAssistantConfig + CMapSettings)
    config/configio.cpp
    config/assistantconfig.cpp
    managers/mapsettings.cpp
    model/commonitems/finditem.cpp
    model/commonitems/ignoreitem.cpp
    # the client token parser (finditem.cpp; the real implementation; the
    # harness links xuocore's sources directly instead of the library)
    ../../xuocore/text_parser.cpp
    # the assistant tokenizer (core/textconv.cpp; the tokenizer parity suite
    # exercises ParseTextTokens - the old CTextParser's line-scoped replacement)
    core/textconv.cpp
    # The REAL text-command table (registration + RunCommand lookup), compiled
    # WITHOUT XUO_ASSISTANT_HARNESS so it sees the real world headers. Its
    # handlers are registered but never invoked by the test (g_World is null,
    # so they early-return); the link needs their client symbols stubbed,
    # which harnessstubs.cpp does. This covers the "Unknown command:
    # useobject" regression: nothing ever populated the table.
    core/textcommandmanager.cpp
    # the stb-style common/fs.h implementation for this standalone link (the
    # main build gets it from xuocore/common.cpp, which is not linked here)
    script/test/fsimpl.cpp
    # the client data reader/writer (the real implementation; the harness
    # links xuocore's sources directly instead of the library)
    ../../xuocore/mappedfile.cpp
    model/commonitems/gumpitem.cpp
    model/commonitems/menu.cpp
    model/commonitems/menuitem.cpp
    model/commonitems/prompt.cpp
    model/commonitems/journalmessage.cpp
    script/quickjs/dtoa.c
    script/quickjs/libregexp.c
    script/quickjs/libunicode.c
    script/quickjs/quickjs.c
    # the engine + handle lifecycle (the run/terminate suite drives the real
    # CScriptHandle flow: thread, QuickJS runtime, interrupt-based terminate)
    script/engine.cpp
    script/scripthandle.cpp
  )
  target_compile_definitions(xuoassist-script-test PRIVATE XUO_ASSISTANT=1 XUO_ASSISTANT_HARNESS=1)
  # the real text-command table must see the real world headers (the harness
  # define globally swaps in the stand-ins)
  set_source_files_properties(core/textcommandmanager.cpp PROPERTIES
    COMPILE_OPTIONS "-UXUO_ASSISTANT_HARNESS")
  set_source_files_properties(
    script/quickjs/dtoa.c
    script/quickjs/libregexp.c
    script/quickjs/libunicode.c
    script/quickjs/quickjs.c
    PROPERTIES COMPILE_OPTIONS "-w"
  )
  set_property(TARGET xuoassist-script-test PROPERTY POSITION_INDEPENDENT_CODE ON)
  target_include_directories(xuoassist-script-test PRIVATE
    ${XUOASSIST_INCLUDE} ${COMMON_INCLUDE} ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(xuoassist-script-test PRIVATE ${COMMON_PROJECT} m pthread)
endif()

# icon-atlas assets + the About logo: compiled into the binary via the
# create_resource macro (CMake/Embedding.cmake, the resources/cover.png
# pattern) so the client ships as a single file - nothing is copied next to
# the exe. One generated header per png (asset_<file>.h), read back by
# ui/assets.cpp (memory-first, the disk lookup stays as the fallback).
# create_resource runs at configure time, so CMAKE_CONFIGURE_DEPENDS re-runs
# the configure step when a png changes and the headers are regenerated.
file(GLOB XUOASSIST_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.png")
set(XUOASSIST_ASSET_HEADERS "")
foreach(asset ${XUOASSIST_ASSETS})
  get_filename_component(name "${asset}" NAME)
  # the symbol inside the header is create_resource's own name mangling
  string(REGEX REPLACE "\\.| |-" "_" symbol "${name}")
  set(header "${CMAKE_CURRENT_BINARY_DIR}/generated/asset_${symbol}.h")
  create_resource("${asset}" "${header}")
  list(APPEND XUOASSIST_ASSET_HEADERS "${header}")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${XUOASSIST_ASSETS})
target_sources(xuoassist PRIVATE ui/assets.cpp)
target_include_directories(xuoassist PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
