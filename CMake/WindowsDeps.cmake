# Download necessary binary files and dependencies to build and run on windows

function(download_package package_name)
    if (NOT EXISTS "${PROJECT_SOURCE_DIR}/${package_name}")
        message(STATUS "Downloading package: ${package_name}.zip...")
        file(DOWNLOAD
            https://github.com/crossuo/dep-build-win/raw/master/${package_name}.zip
            "${CMAKE_BINARY_DIR}/downloads/${package_name}.zip" SHOW_PROGRESS)
        execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf "${CMAKE_BINARY_DIR}/downloads/${package_name}.zip"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    endif()
    message(STATUS "Using bundled binaries at ${PROJECT_SOURCE_DIR}/${package_name}")
endfunction()

if (WIN32)
    if (NOT EXISTS ${PROJECT_SOURCE_DIR}/Dependencies/x64/lib/libcurl.lib)
        download_package("Dependencies")
    endif()
endif()
