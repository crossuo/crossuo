set(PROJECT_NAME external_gfx)
project(${PROJECT_NAME})

set(EXTERNALGFX_PROJECT "${PROJECT_NAME}" CACHE INTERNAL "")
set(EXTERNALGFX_DIR "${PROJECT_SOURCE_DIR}" CACHE INTERNAL "")
set(EXTERNALGFX_INCLUDE "${CMAKE_SOURCE_DIR}" CACHE INTERNAL "")

file(GLOB EXTERNALGFX_HEADERS ${EXTERNALGFX_DIR}/*.h ${EXTERNALGFX_DIR}/imgui/*.h)

set(EXTERNALGFX_SRC
  gfxpp.cpp
  gfx.c
  imgui/imgui.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_mainwindow.cpp
  imgui/imgui_richtext.cpp
  imgui/imgui_impl_sdl.cpp
  imgui/imgui_impl_opengl2.cpp
)

add_library(${PROJECT_NAME} STATIC ${EXTERNALGFX_SRC} ${EXTERNALGFX_HEADERS})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
# Remove warning from 3rd party compilation
if(MSVC)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wno-unused-function" "-Wno-float-equal" "-Wno-shadow")
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    else()
        # GCC
    endif()
endif()
