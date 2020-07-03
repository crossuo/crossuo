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
  uo_installer.cpp
  uo_installer.h
  uop.cpp
  uodata.cpp
  mappedfile.cpp
  client_info.cpp
)

add_library(${PROJECT_NAME} STATIC ${XUOCORE_SRCS} ${XUOCORE_HEADERS})
add_dependencies(${PROJECT_NAME} ${EXTERNAL_PROJECT} ${EXTERNAL_PROJECT} ${COMMON_PROJECT})

target_link_libraries(${PROJECT_NAME} PUBLIC ${EXTERNAL_PROJECT} ${COMMON_PROJECT} ${LOADER} ${THREAD})
target_include_directories(${PROJECT_NAME} PUBLIC ${EXTERNAL_INCLUDE})

set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
