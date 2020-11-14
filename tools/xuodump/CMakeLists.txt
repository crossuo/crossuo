# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

set(PROJECT_NAME xuodump)
project(${PROJECT_NAME})

set(XUODUMP_SRCS
  "xuodump.cpp"
)

add_executable(${PROJECT_NAME} ${XUODUMP_SRCS})

add_dependencies(${PROJECT_NAME} ${EXTERNAL_PROJECT} ${XUOCORE_PROJECT} ${COMMON_PROJECT})
target_link_libraries(${PROJECT_NAME} PUBLIC ${XUOCORE_PROJECT} ${COMMON_PROJECT} ${LOADER} ${THREAD})
target_include_directories(${PROJECT_NAME} PUBLIC ${COMMON_INCLUDE})
