set(PROJECT_NAME xuodump)
project(${PROJECT_NAME})

set(XUODUMP_SRCS
  "xuodump.cpp"
)

add_executable(${PROJECT_NAME} ${XUODUMP_SRCS})

add_dependencies(${PROJECT_NAME} ${EXTERNAL_PROJECT} ${XUOCORE_PROJECT})
target_link_libraries(${PROJECT_NAME} PUBLIC ${LOADER} ${THREAD} ${XUOCORE_PROJECT})
target_include_directories(${PROJECT_NAME} PUBLIC ${COMMON_INCLUDE})
