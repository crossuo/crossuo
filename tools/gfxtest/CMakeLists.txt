# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

add_executable(gfxtest main.cpp)
add_dependencies(gfxtest ${EXTERNAL_PROJECT} ${EXTERNALGFX_PROJECT} ${3RDPARTY_STATIC})

target_link_libraries(gfxtest PUBLIC ${EXTERNAL_PROJECT} ${EXTERNALGFX_PROJECT} ${SDL2_LIBRARIES} ${GL_LIBRARIES} ${LOADER} ${THREAD})
target_include_directories(gfxtest PUBLIC ${EXTERNAL_INCLUDE} ${EXTERNALGFX_INCLUDE} ${SDL2_INCLUDE_DIR})
xuo_disable_console(gfxtest)