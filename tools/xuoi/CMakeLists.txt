# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

set(PROJECT_NAME xuoi)
project(${PROJECT_NAME})

#
# Installer
#
set(XUOI_PROJECT "${PROJECT_NAME}" CACHE INTERNAL "")
set(XUOI_DIR "${PROJECT_SOURCE_DIR}" CACHE INTERNAL "")
set(XUOI_INCLUDE "${XUOI_DIR}" CACHE INTERNAL "")
set(XUOI_LIBS "${PROJECT_NAME} ${THREAD} ${LOADER}" CACHE INTERNAL "")

set(XUOI_SRCS
  xuo_updater.cpp
  xuo_updater.h
  keypatch.cpp
  keypatch.h
  xuoi.cpp
)

add_executable(${PROJECT_NAME} ${XUOI_SRCS})
add_dependencies(${PROJECT_NAME} ${XUOCORE_PROJECT} ${3RDPARTY_STATIC})

target_link_libraries(${PROJECT_NAME} PUBLIC ${XUOCORE_PROJECT} ${CURL_LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC ${XUOCORE_PROJECT})

#
# shardchk
#
set(XUOL_SRCS
  shards.cpp
)

set(SHARD_PROJECT shardchk)

add_executable(${SHARD_PROJECT} ${XUOL_SRCS})
target_compile_definitions(${SHARD_PROJECT} PRIVATE -DVALIDATOR)
add_dependencies(${SHARD_PROJECT} ${XUOCORE_PROJECT} ${EXTERNAL_PROJECT} ${COMMON_PROJECT})

target_link_libraries(${SHARD_PROJECT} PUBLIC ${XUOCORE_PROJECT} ${EXTERNAL_PROJECT} ${COMMON_PROJECT} ${LOADER} ${THREAD})
target_include_directories(${SHARD_PROJECT} PUBLIC ${XUOCORE_PROJECT} ${EXTERNAL_INCLUDE} ${COMMON_INCLUDE})


#
# client_info
#
set(CLIENT_INFO_SRCS
  client_info.cpp
)

set(CLIENT_INFO_PROJECT client_info)

add_executable(${CLIENT_INFO_PROJECT} ${CLIENT_INFO_SRCS})
target_compile_definitions(${CLIENT_INFO_PROJECT} PRIVATE -DSTANDALONE)
add_dependencies(${CLIENT_INFO_PROJECT} ${XUOCORE_PROJECT})

target_link_libraries(${CLIENT_INFO_PROJECT} PUBLIC ${XUOCORE_PROJECT})
target_include_directories(${CLIENT_INFO_PROJECT} PUBLIC ${XUOCORE_PROJECT})

#
# virtual installation
#
set(VUOI_SRCS
  vuoi.cpp
)

set(VUOI_PROJECT vuoi)

add_executable(${VUOI_PROJECT} ${VUOI_SRCS})
add_dependencies(${VUOI_PROJECT} ${XUOCORE_PROJECT} ${3RDPARTY_STATIC})

target_link_libraries(${VUOI_PROJECT} PUBLIC ${XUOCORE_PROJECT} ${CURL_LIBRARIES})
target_include_directories(${VUOI_PROJECT} PUBLIC ${XUOCORE_PROJECT})
