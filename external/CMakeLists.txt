# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

set(PROJECT_NAME external)
project(${PROJECT_NAME})

set(EXTERNAL_PROJECT "${PROJECT_NAME}" CACHE INTERNAL "")
set(EXTERNAL_DIR "${PROJECT_SOURCE_DIR}" CACHE INTERNAL "")
set(EXTERNAL_INCLUDE "${CMAKE_SOURCE_DIR}" CACHE INTERNAL "")

file(GLOB EXTERNAL_HEADERS ${EXTERNAL_DIR}/*.h)

set(EXTERNAL_SRC
  miniconv.c
  tinyfiledialogs.c
  external.cpp
  tracy/TracyClient.cpp
  zlib_amalg.c
)

add_library(${PROJECT_NAME} STATIC ${EXTERNAL_SRC} ${EXTERNAL_HEADERS})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
