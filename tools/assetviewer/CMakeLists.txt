# SPDX-License-Identifier: AGPL-3.0-or-later

# Standalone Asset Viewer application (xuoassetviewer-app): the in-client asset
# browser built as its own executable. It links the shared viewer core
# (src/AssetViewer, WITHOUT the client host), the xuocore data readers and the
# ImGui/window stack, and opens any UO data folder through the SDL3 folder
# dialog. Desktop GL builds only; nothing is shared at runtime (separate
# process, own SDL window).

set(ASSETVIEWER_APP_PROJECT "xuoviewer")
project(${ASSETVIEWER_APP_PROJECT})

set(ASSETVIEWER_SRCS
  main.cpp
  app.h
  host.cpp
  audio.cpp
  audio.h
)

# the viewer core TUs: the client host (AssetViewerClientHost.cpp) is
# deliberately left out - host.cpp below provides the standalone host
set(ASSETVIEWER_CORE_SRCS
  ${CMAKE_SOURCE_DIR}/src/AssetViewer/AssetViewer.cpp
  ${CMAKE_SOURCE_DIR}/src/AssetViewer/AssetGrid.cpp
  ${CMAKE_SOURCE_DIR}/src/AssetViewer/AssetDecode.cpp
  ${CMAKE_SOURCE_DIR}/src/AssetViewer/AssetTextureCache.cpp
)

add_executable(${ASSETVIEWER_APP_PROJECT} ${ASSETVIEWER_SRCS} ${ASSETVIEWER_CORE_SRCS})

# the viewer core is written against the assistant configuration gate
target_compile_definitions(${ASSETVIEWER_APP_PROJECT} PRIVATE XUO_ASSISTANT=1)

add_dependencies(${ASSETVIEWER_APP_PROJECT}
  ${EXTERNAL_PROJECT} ${EXTERNALGFX_PROJECT} ${XUOCORE_PROJECT} ${COMMON_PROJECT} ${3RDPARTY_STATIC})

target_link_libraries(${ASSETVIEWER_APP_PROJECT} PUBLIC
  ${EXTERNAL_PROJECT}
  ${EXTERNALGFX_PROJECT}
  ${XUOCORE_PROJECT}
  ${COMMON_PROJECT}
  ${SDL3_LIBRARIES}
  ${GL_LIBRARIES}
  ${THREAD}
  ${CURL_LIBRARIES}
)

target_include_directories(${ASSETVIEWER_APP_PROJECT} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src/AssetViewer
  ${EXTERNALGFX_INCLUDE}
  ${COMMON_INCLUDE}
)

if (APPLE)
  # apple ld64 spells it -undefined,error (see the matching guard in
  # src/CMakeLists.txt for the crossuo target)
  target_link_options(${ASSETVIEWER_APP_PROJECT} PRIVATE "LINKER:-undefined,error")
elseif (NOT XUO_WINDOWS AND NOT XUO_EMSC)
  target_link_options(${ASSETVIEWER_APP_PROJECT} PRIVATE "LINKER:--no-undefined")
endif()

if (XUO_WINDOWS)
  target_sources(${ASSETVIEWER_APP_PROJECT} PRIVATE ${CMAKE_SOURCE_DIR}/resources/client.rc)
  # client.rc includes src/targetver.h; windres picks the include dirs up
  # from this target's INTERFACE/PUBLIC include path
  target_include_directories(${ASSETVIEWER_APP_PROJECT} PRIVATE ${CMAKE_SOURCE_DIR}/src)
endif()

xuo_disable_console(${ASSETVIEWER_APP_PROJECT})
