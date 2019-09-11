
#include "uolib.h"

#define FS_LOG_DEBUG(...) DEBUG(Filesystem, __VA_ARGS__)
#define FS_LOG_ERROR(...) Error(Filesystem, __VA_ARGS__)

#define FS_IMPLEMENTATION
#include <common/fs.h>
