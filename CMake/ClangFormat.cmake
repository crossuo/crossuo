# From Yuzu: https://github.com/yuzu-emu/yuzu/blob/master/CMakeLists.txt
#
# Setup a custom clang-format target (if clang-format can be found) that will run
# against all the src files. This should be used before making a pull request.
# =======================================================================

set(CLANG_FORMAT_POSTFIX "-7")
find_program(CLANG_FORMAT
    NAMES
      clang-format${CLANG_FORMAT_POSTFIX}
      clang-format
    PATHS
      /usr/bin
      ${PROJECT_BINARY_DIR}/downloads
      $ENV{PATH}
    )
if (NOT CLANG_FORMAT)
    if (WIN32)
        message(STATUS "Clang format not found! Downloading...")
        set(CLANG_FORMAT "${PROJECT_BINARY_DIR}/downloads/clang-format${CLANG_FORMAT_POSTFIX}.exe")
        file(DOWNLOAD
            https://github.com/crossuo/dep-build-win/raw/master/clang-format${CLANG_FORMAT_POSTFIX}.exe
            "${CLANG_FORMAT}" SHOW_PROGRESS
            STATUS DOWNLOAD_SUCCESS)
        if (NOT DOWNLOAD_SUCCESS EQUAL 0)
            message(WARNING "Could not download clang format! Disabling the clang format target")
            file(REMOVE "${CLANG_FORMAT}")
            unset(CLANG_FORMAT)
        endif()
    else()
        message(WARNING "Clang format not found! Disabling the clang format target")
    endif()
endif()

if (CLANG_FORMAT)
    set(SRCS ${PROJECT_SOURCE_DIR})
    set(CCOMMENT "Running clang format against all the .h and .cpp files in src/")
    if (WIN32)
        add_custom_target(clang-format
			WORKING_DIRECTORY ${SRCS}
            COMMAND powershell.exe -Command \"Get-ChildItem tools/* -Include *.cpp,*.h -Recurse | Foreach {${CLANG_FORMAT} --style=file -i $_.fullname}\"
            COMMAND powershell.exe -Command \"Get-ChildItem common/* -Include *.cpp,*.h -Recurse | Foreach {${CLANG_FORMAT} --style=file -i $_.fullname}\"
            COMMAND powershell.exe -Command \"Get-ChildItem xuocore/* -Include *.cpp,*.h -Recurse | Foreach {${CLANG_FORMAT} --style=file -i $_.fullname}\"
            COMMAND powershell.exe -Command \"Get-ChildItem src/* -Include *.cpp,*.h -Recurse | Foreach {${CLANG_FORMAT} --style=file -i $_.fullname}\"
            COMMENT ${CCOMMENT})
    elseif(MINGW)
        add_custom_target(clang-format
			WORKING_DIRECTORY ${SRCS}
            COMMAND find `cygpath -u tools/` -iname *.h -o -iname *.cpp | xargs `cygpath -u ${CLANG_FORMAT}` --style=file -i
            COMMAND find `cygpath -u common/` -iname *.h -o -iname *.cpp | xargs `cygpath -u ${CLANG_FORMAT}` --style=file -i
            COMMAND find `cygpath -u xuocore/` -iname *.h -o -iname *.cpp | xargs `cygpath -u ${CLANG_FORMAT}` --style=file -i
            COMMAND find `cygpath -u src/` -iname *.h -o -iname *.cpp | xargs `cygpath -u ${CLANG_FORMAT}` --style=file -i
            COMMENT ${CCOMMENT})
    else()
        add_custom_target(clang-format
			WORKING_DIRECTORY ${SRCS}
            COMMAND "${CLANG_FORMAT}" --style=file -i tools/**/**.{h,cpp}
            COMMAND "${CLANG_FORMAT}" --style=file -i common/*.{h,cpp}
            COMMAND "${CLANG_FORMAT}" --style=file -i xuocore/*.{h,cpp}
            COMMAND "${CLANG_FORMAT}" --style=file -i src/**/**.{h,cpp}
            COMMENT ${CCOMMENT})
    endif()
    unset(SRCS)
    unset(CCOMMENT)
endif()
