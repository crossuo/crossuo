# SPDX-License-Identifier: AGPL-3.0-or-later

# In-client Asset Viewer: a read-only ImGui browser for the UO data files,
# linked into the crossuo target (wired by src/CMakeLists.txt, built only in
# the same GL3/GLES2 configurations as the assistant whose ImGui frame
# pipeline hosts the viewer window)

set(XUOASSETVIEWER_PROJECT "xuoassetviewer" CACHE INTERNAL "")
set(XUOASSETVIEWER_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "")

set(XUOASSETVIEWER_SRCS
  "AssetViewer.cpp"
  "AssetGrid.cpp"
  "AssetDecode.cpp"
  "AssetTextureCache.cpp"
)

add_library(xuoassetviewer STATIC ${XUOASSETVIEWER_SRCS})

# the define lives on this target for its own sources and on crossuo for the
# hook call sites (mirrors xuoassist)
target_compile_definitions(xuoassetviewer PRIVATE XUO_ASSISTANT=1)
# linked into the single self-contained client executable, so it needs
# position independent code
set_property(TARGET xuoassetviewer PROPERTY POSITION_INDEPENDENT_CODE ON)

add_dependencies(xuoassetviewer ${EXTERNALGFX_PROJECT} ${XUOCORE_PROJECT} ${COMMON_PROJECT})
target_link_libraries(xuoassetviewer PUBLIC ${EXTERNALGFX_PROJECT} ${XUOCORE_PROJECT} ${COMMON_PROJECT})
target_include_directories(xuoassetviewer PUBLIC
  ${XUOASSETVIEWER_INCLUDE}
  ${EXTERNALGFX_INCLUDE}
  ${COMMON_INCLUDE}
  ${CMAKE_SOURCE_DIR}/src
)
