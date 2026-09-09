# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

set(PROJECT_NAME xuocore)
project(${PROJECT_NAME})

set(XUOCORE_PROJECT "${PROJECT_NAME}" CACHE INTERNAL "")
set(XUOCORE_DIR "${PROJECT_SOURCE_DIR}" CACHE INTERNAL "")
set(XUOCORE_INCLUDE "${XUOCORE_DIR}" CACHE INTERNAL "")

file(GLOB XUOCORE_HEADERS ${XUOCORE_DIR}/*.h)

set(XUOCORE_SRCS
  common.cpp
  common.h
  http.cpp
  http.h
  mft.cpp
  mft.h
  meta.cpp
  meta.h
  vfs.cpp
  vfs.h
  vfs_fetch.cpp
  vfs_fetch.h
  vfs_manifest.cpp
  vfs_manifest.h
  vfs_manifest_embedded.cpp
  vfs_record.cpp
  vfs_record.h
  vfs_state.cpp
  vfs_state.h
  vfs_store.cpp
  vfs_store.h
  vfs_store_arena.cpp
  vfs_store_arena.h
  vfs_stub.cpp
  vfs_stub.h
  vfs_transport.cpp
  vfs_transport.h
  vfs_trap.cpp
  vfs_trap.h
  xuoi.cpp
  xuoi.h
  uo_installer.cpp
  uo_installer.h
  uop.cpp
  uodata.cpp
  mappedfile.cpp
  client_info.cpp
  text_parser.cpp
)

add_library(${PROJECT_NAME} STATIC ${XUOCORE_SRCS} ${XUOCORE_HEADERS})
add_dependencies(${PROJECT_NAME} ${EXTERNAL_PROJECT} ${EXTERNAL_PROJECT} ${COMMON_PROJECT})

target_link_libraries(${PROJECT_NAME} PUBLIC ${EXTERNAL_PROJECT} ${COMMON_PROJECT} ${LOADER} ${THREAD})
target_include_directories(${PROJECT_NAME} PUBLIC ${EXTERNAL_INCLUDE})

set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
