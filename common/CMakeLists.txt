set(PROJECT_NAME common)
project(${PROJECT_NAME})

set(COMMON_PROJECT "${PROJECT_NAME}" CACHE INTERNAL "")
set(COMMON_DIR "${PROJECT_SOURCE_DIR}" CACHE INTERNAL "")
set(COMMON_INCLUDE "${CMAKE_SOURCE_DIR}" CACHE INTERNAL "")

file(GLOB COMMON_HEADERS ${EXTERNAL_DIR}/*.h)

set(COMMON_SRC
  str.h
  fs.h
  utils.h
  checksum.h
  logging/loggers.h
  logging/logging.h
  logging/logging.cpp
)

add_library(${PROJECT_NAME} STATIC ${COMMON_SRC} ${COMMON_HEADERS})
add_dependencies(${PROJECT_NAME} ${EXTERNAL_PROJECT})
target_link_libraries(${PROJECT_NAME} PUBLIC ${EXTERNAL_LIBS} ${CURL_LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC ${EXTERNAL_INCLUDE})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)