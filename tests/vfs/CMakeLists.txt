# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 CrossUO Team

#
# vfs tests (VUOI streaming-install foundation)
#
set(PROJECT_NAME vfs_tests)
project(${PROJECT_NAME})

set(VFS_TESTS_SRCS
  test_main.cpp
  test_main.h
  test_fetch.cpp
  test_plan.cpp
  test_product.h
  test_state.cpp
  test_trap.cpp
  test_store.cpp
  test_urls.cpp
  test_uop_parse.cpp
)

add_executable(${PROJECT_NAME} ${VFS_TESTS_SRCS})
add_dependencies(${PROJECT_NAME} ${XUOCORE_PROJECT} ${EXTERNAL_PROJECT} ${COMMON_PROJECT})
target_link_libraries(${PROJECT_NAME} PUBLIC ${XUOCORE_PROJECT} ${CURL_LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC ${XUOCORE_PROJECT} ${EXTERNAL_INCLUDE} ${COMMON_INCLUDE})
target_compile_definitions(
  ${PROJECT_NAME} PRIVATE
  VFS_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
  VFS_SERVER_SCRIPT="${CMAKE_CURRENT_SOURCE_DIR}/http_server.py"
)
