set(PROJECT_NAME xuodump)
project(${PROJECT_NAME})

set(XUODUMP_SRCS
  "xuodump.cpp"
)

add_executable(${PROJECT_NAME} ${XUODUMP_SRCS})

add_dependencies(${PROJECT_NAME} ${EXTERNAL_PROJECT} ${XUOCORE_PROJECT})
target_link_libraries(${PROJECT_NAME} PUBLIC ${LOADER} ${THREAD} ${XUOCORE_PROJECT})
target_include_directories(${PROJECT_NAME} PUBLIC ${COMMON_INCLUDE})

if (ENABLE_COTIRE)
  set_target_properties(${PROJECT_NAME} PROPERTIES COTIRE_ENABLE_PRECOMPILED_HEADER FALSE)
  set_target_properties(${PROJECT_NAME} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "")
  cotire(${PROJECT_NAME})
endif()
