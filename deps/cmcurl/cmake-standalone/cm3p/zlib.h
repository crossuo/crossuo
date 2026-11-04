/* XUO standalone shim: upstream includes <cm3p/zlib.h> when built inside
 * the CMake tree (bundled cmzlib); a standalone build uses the system zlib
 * passed via ZLIB_LIBRARY. */
#include <zlib.h>
