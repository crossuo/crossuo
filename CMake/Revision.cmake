# for revision info
find_package(Git)
if(GIT_FOUND)
  # make sure version information gets re-run when the current Git HEAD changes
  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse --git-path HEAD
      OUTPUT_VARIABLE crossuo_git_head_filename
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${crossuo_git_head_filename}")

  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse --symbolic-full-name HEAD
      OUTPUT_VARIABLE crossuo_git_head_symbolic
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      COMMAND ${GIT_EXECUTABLE} rev-parse --git-path ${crossuo_git_head_symbolic}
      OUTPUT_VARIABLE crossuo_git_head_symbolic_filename
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${crossuo_git_head_symbolic_filename}")

  # defines XUO_WC_REVISION
  EXECUTE_PROCESS(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
      OUTPUT_VARIABLE XUO_WC_REVISION
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  # defines XUO_WC_DESCRIBE
  EXECUTE_PROCESS(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} describe --always --long --dirty
      OUTPUT_VARIABLE XUO_WC_DESCRIBE
      OUTPUT_STRIP_TRAILING_WHITESPACE)

  # remove hash (and trailing "-0" if needed) from description
  STRING(REGEX REPLACE "(-0)?-[^-]+((-dirty)?)$" "\\2" XUO_WC_DESCRIBE "${XUO_WC_DESCRIBE}")

  # defines XUO_WC_BRANCH
  EXECUTE_PROCESS(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
      OUTPUT_VARIABLE XUO_WC_BRANCH
      OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# version number
set(XUO_VERSION_MAJOR "0")
set(XUO_VERSION_MINOR "1")
if(XUO_WC_BRANCH STREQUAL "stable")
  set(XUO_VERSION_PATCH "0")
else()
  set(XUO_VERSION_PATCH ${XUO_WC_REVISION})
endif()

# If this is not built from a Git repository, default the version info to
# reasonable values.
if(NOT XUO_WC_REVISION)
  set(XUO_WC_DESCRIBE "${XUO_VERSION_MAJOR}.${XUO_VERSION_MINOR}")
  set(XUO_WC_REVISION "${XUO_WC_DESCRIBE} (unk)")
  set(XUO_WC_BRANCH "master")
endif()

if(XUO_WC_BRANCH STREQUAL "master" OR XUO_WC_BRANCH STREQUAL "stable")
  set(XUO_WC_IS_STABLE "1")
else()
  set(XUO_WC_IS_STABLE "0")
endif()
