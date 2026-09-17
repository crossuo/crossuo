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
  "script/scriptgump.cpp"
  "script/scriptstate.cpp"
  "script/scripthandle.cpp"
  "ui/scriptmenu.cpp"
  "ui/editor.cpp"
  "ui/scripteditor.cpp"
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
  "model/commonitems/displayitem.cpp"
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
# linked into the client binary (the shared libcrossuo.so of the
# XUO_ASSISTANT=OFF layout, or the single-executable assistant build),
# so it needs position independent code either way
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
    model/commonitems/displayitem.cpp
    # the client token parser (finditem.cpp; the real implementation; the
    # harness links xuocore's sources directly instead of the library)
    ../../xuocore/text_parser.cpp
    # the assistant tokenizer (core/textconv.cpp; the tokenizer parity suite
    # exercises ParseTextTokens - the old CTextParser's line-scoped replacement)
    core/textconv.cpp
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
  )
  target_compile_definitions(xuoassist-script-test PRIVATE XUO_ASSISTANT=1 XUO_ASSISTANT_HARNESS=1)
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

# # icon-atlas assets. The copy lives on the crossuo POST_BUILD
# (not on this static lib: a $<TARGET_FILE_DIR:crossuo> here would create a
# dependency cycle xuoassist -> crossuo -> xuoassist). Destination is the
# runtime lookup path - <exe dir>/assets/xuoassist, i.e. <build>/src/assets/
# xuoassist since the single-executable change; a copy into CMAKE_BINARY_DIR
# landed a level away from the ExeFilePath lookup ("about logo not found:
# .../src/assets/xuoassist/tray.png"). Covers every png of the dir: the ~14
# atlas icons (the same IconAssetPath lookup) and the About logo.
