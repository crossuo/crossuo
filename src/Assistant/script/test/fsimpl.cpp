// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The stb-style common/fs.h implementation for the script-test harness link
// only: the main build compiles it in xuocore/common.cpp, which the harness
// does not link. ScriptTextWindow_SaveToFile -> AtomicFileWrite needs the
// fs_path/fs_open/fs_del pieces in this standalone binary.

#define FS_IMPLEMENTATION
#include <common/fs.h>
