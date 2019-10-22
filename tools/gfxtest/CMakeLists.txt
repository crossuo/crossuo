add_executable(gfxtest main.cpp)
add_dependencies(gfxtest ${EXTERNAL_PROJECT} ${EXTERNALGFX_PROJECT})

target_link_libraries(gfxtest PUBLIC ${EXTERNAL_PROJECT} ${SDL2_LIBRARIES} ${EXTERNALGFX_PROJECT} ${GL_LIBRARIES} ${LOADER} ${THREAD})
target_include_directories(gfxtest PUBLIC ${EXTERNAL_INCLUDE} ${EXTERNALGFX_INCLUDE})
