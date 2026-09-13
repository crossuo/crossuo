# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

# X:UO Assist: in-client ImGui assistant, linked into the crossuo target
# (see the master plan ADR D9; wired by src/CMakeLists.txt, guarded by
# the root CMakeLists XUO_ASSISTANT / XUO_ASSISTANT_ENABLED)

set(XUOASSIST_PROJECT "xuoassist" CACHE INTERNAL "")
set(XUOASSIST_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "")

set(XUOASSIST_SRCS
  "AssistantWindow.cpp"
  # Phase 2: the assistant shell, widget kit and the simple tabs (§6/§7)
  "ui/assistantui.cpp"
  "ui/assistanteventsui.cpp"
  "ui/widgets.cpp"
  "ui/tabmain.cpp"
  "ui/tablists.cpp"
  "ui/tabfilters.cpp"
  # Phase 3: hotkeys & macros (§7 items 1-4)
  "core/hotkeys.cpp"
  "core/actions.cpp"
  "macros/macroexecutor.cpp"
  "ui/hotkeysform.cpp"
  "ui/tabhotkeys.cpp"
  "ui/tabmacros.cpp"
  # Phase 4a: the agent tabs, skills and display (§2.3/§2.8/§2.9, §7 items 1-3)
  "core/dressagent.cpp"
  "core/shopagent.cpp"
  "ui/tabdress.cpp"
  "ui/tabparty.cpp"
  "ui/tabshop.cpp"
  "ui/tabskills.cpp"
  "ui/tabdisplay.cpp"
  # Phase 4b: the script runtime (QuickJS-ng vendored under script/quickjs,
  # see script/quickjs/VENDOR.md), the Scripts tab and the script editor
  "script/commandqueue.cpp"
  "script/journal.cpp"
  "script/engine.cpp"
  "script/apiscript.cpp"
  "script/scripthandle.cpp"
  "ui/scriptmenu.cpp"
  "ui/editor.cpp"
  "ui/scripteditor.cpp"
  "ui/tabscripts.cpp"
  # Phase 1a: Qt-free foundation ported from xuoassist (plan §7 Phase 1 item 1)
  "core/global.cpp"
  "core/textconv.cpp"
  "core/textparser.cpp"
  "core/assistantevents.cpp"
  "core/packets.cpp"
  # Phase 1b: packet pipeline (plan §7 Phase 1 items 2/4)
  "managers/packetmanager.cpp"
  # Phase 1c: assistant core (plan §7 Phase 1 items 5/6)
  "core/assist.cpp"
  "core/target.cpp"
  "core/textcommandmanager.cpp"
  "managers/clientmacromanager.cpp"
  "managers/contextmenumanager.cpp"
  "managers/corpsemanager.cpp"
  "managers/displaymanager.cpp"
  "managers/gumpmanager.cpp"
  "managers/guildmanager.cpp"
  "managers/menumanager.cpp"
  "managers/objectpropertiesmanager.cpp"
  "managers/partymanager.cpp"
  "managers/promptmanager.cpp"
  "managers/skillmanager.cpp"
  "managers/spellmanager.cpp"
  "managers/trademanager.cpp"
  "macros/macro.cpp"
  "config/xml.cpp"
  "config/assistantconfig.cpp"
  "model/basequeue.cpp"
  "model/gameobjects/gameobject.cpp"
  "model/gameobjects/gameitem.cpp"
  "model/gameobjects/gamecharacter.cpp"
  "model/gameobjects/gameplayer.cpp"
  "model/gameobjects/gameworld.cpp"
  "model/commonitems/finditem.cpp"
  "model/commonitems/ignoreitem.cpp"
  "model/commonitems/buffitem.cpp"
  "model/commonitems/displayitem.cpp"
  "model/commonitems/gump.cpp"
  "model/commonitems/gumphook.cpp"
  "model/commonitems/gumpitem.cpp"
  "model/commonitems/journalmessage.cpp"
  "model/commonitems/menu.cpp"
  "model/commonitems/menuitem.cpp"
  "model/commonitems/prompt.cpp"
  "model/commonitems/shopitem.cpp"
  "model/commonitems/skill.cpp"
  "model/commonitems/tradewindow.cpp"
  "model/commonitems/vendorshopitem.cpp"
  "model/datareader.cpp"
  "model/datawriter.cpp"
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
# the define lives on this target for its own sources (mirrors the launcher's
# target-scoped define on crossuo) and on crossuo for the hook call sites
target_compile_definitions(xuoassist PRIVATE XUO_ASSISTANT=1)
# linked into the shared libcrossuo.so, so it needs position independent code
set_property(TARGET xuoassist PROPERTY POSITION_INDEPENDENT_CODE ON)

add_dependencies(xuoassist ${EXTERNALGFX_PROJECT} ${XUOCORE_PROJECT} ${COMMON_PROJECT})
target_link_libraries(xuoassist PUBLIC ${EXTERNALGFX_PROJECT} ${XUOCORE_PROJECT} ${COMMON_PROJECT})
target_include_directories(xuoassist PUBLIC
  ${XUOASSIST_INCLUDE}
  ${EXTERNALGFX_INCLUDE}
  ${COMMON_INCLUDE}
  ${CMAKE_SOURCE_DIR}/src
)

# Phase 4b: the script-runtime harness (no client deps; the queue drain and
# the engine flow are stubbed in the harness main)
option(XUO_ASSISTANT_SCRIPT_TESTS "Build the xuoassist script harness" OFF)
if(XUO_ASSISTANT_SCRIPT_TESTS)
  add_executable(xuoassist-script-test
    script/test/harness.cpp
    script/commandqueue.cpp
    script/journal.cpp
    model/commonitems/journalmessage.cpp
    script/quickjs/dtoa.c
    script/quickjs/libregexp.c
    script/quickjs/libunicode.c
    script/quickjs/quickjs.c
  )
  target_compile_definitions(xuoassist-script-test PRIVATE XUO_ASSISTANT=1)
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

# Phase 2: icon-atlas assets (§6). Copied next to the binary so the runtime
# lookup (assets/xuoassist, then next to the exe) finds them in dev builds.
add_custom_command(
  TARGET xuoassist POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/assets"
          "${CMAKE_BINARY_DIR}/assets/xuoassist"
  COMMENT "Copying xuoassist icon assets")
