#include "Stubs.h"

#if !defined(ORION_WINDOWS)

#include <cassert>
#include <cwctype>
#include <errno.h>

// Bad and very ugly "API" stuff
uint32_t DefWindowProc(void *, uint32_t, void *, void *)
{
    NOT_IMPLEMENTED;
    return 0;
}
bool OpenClipboard(void *)
{
    NOT_IMPLEMENTED;
    return false;
}
void *GetClipboardData(unsigned)
{
    NOT_IMPLEMENTED;
    return nullptr;
}
bool CloseClipboard()
{
    NOT_IMPLEMENTED;
    return false;
}
wchar_t *GetCommandLineW()
{
    NOT_IMPLEMENTED;
    return nullptr;
}
const wchar_t **CommandLineToArgvW(wchar_t *, int *)
{
    NOT_IMPLEMENTED;
    return nullptr;
}
int GetSystemDefaultLangID()
{
    NOT_IMPLEMENTED;
    return 0;
}
void *ShellExecuteA(void *, const char *, const char *, const char *, const char *, int)
{
    NOT_IMPLEMENTED;
    return nullptr;
}
void *LocalFree(void *p)
{
    free(p); /*wtf*/
    return nullptr;
}
void *GlobalLock(void *)
{
    NOT_IMPLEMENTED;
    return nullptr;
}
bool GlobalUnlock(void *)
{
    NOT_IMPLEMENTED;
    return false;
}
int GetProfileStringA(const char *, const char *, const char *, char *, int)
{
    NOT_IMPLEMENTED;
    return 0;
}
// Thread
void KillTimer(void *, unsigned int)
{
    NOT_IMPLEMENTED;
}
void SetTimer(void *, unsigned int, unsigned int, void *)
{
    NOT_IMPLEMENTED;
}

#endif
