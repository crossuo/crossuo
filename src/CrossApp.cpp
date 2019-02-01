#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "api/plugininterface.h"

int main(int argc, char **argv)
{
    typedef int (*plugin_main)(int, char **);

    void *so = dlopen(CROSSUO_CLIENT, RTLD_LAZY);
    if (!so)
    {
        fprintf(stderr, "Could not load shared object '%s'\n", CROSSUO_CLIENT);
        fprintf(stderr, "Reason: dlopen failed with: %s\n", dlerror());
        return -1;
    }

    plugin_main run = (plugin_main)dlsym(so, "plugin_main");
    if (!run)
    {
        dlclose(so);
        fprintf(stderr, "'%s' is invalid or corrupted\n", CROSSUO_CLIENT);
    }

    int r = run(argc, argv);
    dlclose(so);
    exit(r);
}
