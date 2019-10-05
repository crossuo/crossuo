// MIT License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#define LOG_DEBUG(...) // comment to enable debug logging
#define LOG_TRACE(...) // comment to enable tracing
#define LOG_IMPLEMENTATION
#include <common/log.h>
// needed by loguru from uocore.lib
const char *log_system_name(int)
{
    return LOG_TAG;
};

#define CHECKSUM_IMPLEMENTATION
#include <common/checksum.h>

#define FS_IMPLEMENTATION
#include <common/fs.h>

#define MINIZ_IMPLEMENTATION
#include <external/miniz.h>

#define LOOKUP3_IMPLEMENTATION
#include <external/lookup3.h>
