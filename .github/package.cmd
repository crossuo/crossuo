:: SPDX-License-Identifier: AGPL-3.0-or-later
:: SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

echo Building %GITHUB_REF_NAME% package
md crossuo-win64-%GITHUB_REF_NAME%
copy build\src\Release\crossuo.exe crossuo-win64-%GITHUB_REF_NAME%\crossuo.exe
copy build\tools\xuoi\Release\xuolauncher.exe crossuo-win64-%GITHUB_REF_NAME%\xuolauncher.exe
7z a crossuo-win64-%GITHUB_REF_NAME%.zip crossuo-win64-v%GITHUB_REF_NAME%\