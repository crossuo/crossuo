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
  common.h
  http.cpp
  http.h
  xuo_updater.cpp
  xuo_updater.h
  mft.cpp
  mft.h
  xuoi.cpp
)

add_executable(${PROJECT_NAME} ${XUOI_SRCS})
add_dependencies(${PROJECT_NAME} ${EXTERNAL_PROJECT} ${XUOCORE_PROJECT} cmcurl)

target_link_libraries(${PROJECT_NAME} PUBLIC ${XUOCORE_PROJECT} ${CURL_LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC ${XUOCORE_PROJECT})

#
# Launcher
#
set(XUOL_SRCS
  http.cpp
  http.h
  mft.cpp
  mft.h
  common.cpp
  common.h
  xuo_updater.cpp
  xuo_updater.h
  xuolauncher.cpp
  accounts.cpp
  accounts.h
  ui_model.cpp
  ui_model.h
  ui_shards.cpp
  ui_shards.h
  cfg_converters.h
  cfg_crossuo.h
  cfg_launcher.h
  cfg_loader.h
  cfg_shard.h
)

if(XUO_WINDOWS)
  set(XUOL_SRCS ${XUOL_SRCS}
    ${CMAKE_SOURCE_DIR}/resources/launcher.rc
  )
endif()

set(XUOL_PROJECT xuolauncher)

add_executable(${XUOL_PROJECT} ${XUOL_SRCS})
add_dependencies(${XUOL_PROJECT} ${XUOCORE_PROJECT} ${EXTERNALGFX_PROJECT} SDL2-static cmcurl)

target_link_libraries(${XUOL_PROJECT} PUBLIC ${XUOCORE_PROJECT} ${CURL_LIBRARIES} ${EXTERNALGFX_PROJECT} ${SDL2_LIBRARIES} ${GL_LIBRARIES})
target_include_directories(${XUOL_PROJECT} PUBLIC ${XUOCORE_PROJECT} ${EXTERNALGFX_INCLUDE})
xuo_disable_console(${XUOL_PROJECT})

#
# shardchk
#
set(XUOL_SRCS
  common.cpp
  shards.cpp
)

set(SHARD_PROJECT shardchk)

add_executable(${SHARD_PROJECT} ${XUOL_SRCS})
target_compile_definitions(${SHARD_PROJECT} PRIVATE -DVALIDATOR)
add_dependencies(${SHARD_PROJECT} ${EXTERNAL_PROJECT} ${COMMON_PROJECT})

target_link_libraries(${SHARD_PROJECT} PUBLIC ${EXTERNAL_PROJECT} ${COMMON_PROJECT} ${LOADER} ${THREAD})
target_include_directories(${SHARD_PROJECT} PUBLIC ${EXTERNAL_INCLUDE} ${COMMON_INCLUDE})


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
