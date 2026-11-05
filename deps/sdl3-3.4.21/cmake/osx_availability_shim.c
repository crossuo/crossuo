/*
    XUO vendored-SDL3 build helper.

    The vendored SDL3 Objective-C sources use ObjC @available() checks, which
    clang lowers to calls to ___isPlatformVersionAtLeast(). That symbol lives
    in the Darwin compiler-rt builtins library, which the Debian clang used by
    the osxcross toolchain does not ship. Provide the same semantics here:
    compare the running OS version (kern.osproductversion) against the
    requested API version, exactly like compiler-rt's os_version_check.c.
*/
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/sysctl.h>

int32_t __isPlatformVersionAtLeast(int32_t major, int32_t minor, int32_t micro, int32_t pad)
{
    (void)pad;
    static int32_t cached_parts[3] = { -1, -1, -1 };
    if (cached_parts[0] < 0)
    {
        char ver[32] = { 0 };
        size_t sz = sizeof(ver);
        if (sysctlbyname("kern.osproductversion", ver, &sz, NULL, 0) != 0)
        {
            /* unknown host version: assume the APIs are available (the SDK
               matched at build time), matching compiler-rt's fallback */
            cached_parts[0] = INT32_MAX;
        }
        else
        {
            /* parse "12.3" / "12.3.1" */
            long part[3] = { 0, 0, 0 };
            int idx = 0;
            char *p = ver;
            while (*p && idx < 3)
            {
                part[idx] = strtol(p, &p, 10);
                idx++;
                if (*p == '.')
                {
                    p++;
                }
                else
                {
                    break;
                }
            }
            cached_parts[0] = (int32_t)part[0];
            cached_parts[1] = (int32_t)part[1];
            cached_parts[2] = (int32_t)part[2];
        }
    }
    if (cached_parts[0] != major)
    {
        return cached_parts[0] > major ? 1 : 0;
    }
    if (cached_parts[1] != minor)
    {
        return cached_parts[1] > minor ? 1 : 0;
    }
    return cached_parts[2] >= micro ? 1 : 0;
}
