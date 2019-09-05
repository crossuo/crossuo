set(installer_src
  main.cpp
)

add_executable(xuo ${installer_src})
add_dependencies(xuo curl-7.50.1 external)

target_link_libraries(xuo PUBLIC ${EXTERNAL_LIBS} curl)
