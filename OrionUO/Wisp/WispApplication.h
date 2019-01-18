// MIT License

#ifndef WISP_APP_H
#define WISP_APP_H

#include "FileSystem.h"

namespace Wisp
{
class CApplication
{
public:
    os_path m_ExePath;
    os_path m_UOPath;

protected:
    virtual void OnMainLoop() {}

public:
    CApplication();
    virtual ~CApplication();

    virtual void Init();

#if USE_WISP
    HINSTANCE Hinstance = 0;
    int Run(HINSTANCE hinstance);
#else
    int Run();
#endif

    os_path ExeFilePath(const char *str, ...) const;
    os_path UOFilesPath(const char *str, ...) const;
    os_path UOFilesPath(const string &str, ...) const;
};

}; // namespace Wisp

#endif // WISP_APP_H
