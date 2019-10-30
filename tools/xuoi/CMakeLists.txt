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
  common.cpp
  http.cpp
  xuo_updater.cpp
  main.cpp
)

add_executable(${PROJECT_NAME} ${XUOI_SRCS})
add_dependencies(${PROJECT_NAME} ${EXTERNAL_PROJECT} ${CURL_PROJECT})

target_link_libraries(${PROJECT_NAME} PUBLIC ${CURL_LIBRARIES} ${EXTERNAL_PROJECT} ${LOADER} ${THREAD})
target_include_directories(${PROJECT_NAME} PUBLIC ${EXTERNAL_INCLUDE} ${CURL_INCLUDE})

#
# Launcher
#
set(XUOL_SRCS
  common.cpp
  http.cpp
  xuo_updater.cpp
  xuolauncher.cpp
  shards.cpp
)

set(XUOL_PROJECT xuolauncher)

add_executable(${XUOL_PROJECT} ${XUOL_SRCS})
add_dependencies(${XUOL_PROJECT} ${EXTERNAL_PROJECT} ${EXTERNALGFX_PROJECT} ${CURL_PROJECT})

target_link_libraries(${XUOL_PROJECT} PUBLIC ${CURL_LIBRARIES} ${EXTERNAL_PROJECT} ${SDL2_LIBRARIES} ${EXTERNALGFX_PROJECT} ${GL_LIBRARIES} ${LOADER} ${THREAD})
target_include_directories(${XUOL_PROJECT} PUBLIC ${EXTERNAL_INCLUDE} ${EXTERNALGFX_INCLUDE} ${CURL_INCLUDE})
