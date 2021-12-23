:: SPDX-License-Identifier: AGPL-3.0-or-later
:: SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

if %GITHUB_REF_NAME% == master (
  set BUILD=v%GITHUB_REF_NAME%
) else (
  set BUILD=master
)

echo Building %BUILD% package
md crossuo-win64-%BUILD%
copy build\src\Release\crossuo.exe crossuo-win64-%BUILD%\crossuo.exe
copy build\tools\xuoi\Release\xuolauncher.exe crossuo-win64-%BUILD%\xuolauncher.exe
7z a crossuo-win64-%BUILD%.zip crossuo-win64-%BUILD%\