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
