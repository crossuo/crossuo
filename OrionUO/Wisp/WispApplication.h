// MIT License

#pragma once

#include "FileSystem.h"

namespace Wisp
{
class CApplication
{
public:
    os_path m_ExePath;
    os_path m_UOPath;
    string m_ServerAddress;
    uint16_t m_ServerPort = 2593;

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
