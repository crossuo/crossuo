# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

# X:UO Assist: in-client ImGui assistant, linked into the crossuo target
# (see the master plan ADR D9; wired by src/CMakeLists.txt, guarded by
# the root CMakeLists XUO_ASSISTANT / XUO_ASSISTANT_ENABLED)

set(XUOASSIST_PROJECT "xuoassist" CACHE INTERNAL "")
set(XUOASSIST_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "")

set(XUOASSIST_SRCS
  "AssistantWindow.cpp"
  "AssistantUI.cpp"
  # Phase 1a: Qt-free foundation ported from xuoassist (plan §7 Phase 1 item 1)
  "core/global.cpp"
  "core/textconv.cpp"
  "core/textparser.cpp"
  "core/assistantevents.cpp"
  "core/packets.cpp"
  # Phase 1b: packet pipeline (plan §7 Phase 1 items 2/4)
  "managers/packetmanager.cpp"
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
