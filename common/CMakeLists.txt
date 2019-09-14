set(COMMON_INCLUDE "${CMAKE_SOURCE_DIR}" CACHE INTERNAL "")

set(COMMON_FILES
  str.h
  fs.h
  utils.h
  checksum.h
)

if(XUO_WINDOWS)
    # HACK to get these files to show in visual studio
    add_custom_target(common "echo" SOURCES ${COMMON_FILES})
endif()
