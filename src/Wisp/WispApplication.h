// MIT License

#ifndef WISP_APP_H
#define WISP_APP_H

#include <common/fs.h>

namespace Wisp
{
class CApplication
{
public:
    fs_path m_ExePath;
    fs_path m_UOPath;

protected:
    virtual void OnMainLoop() {}

public:
    CApplication();
    virtual ~CApplication();
    virtual void Init();
    int Run();
    fs_path ExeFilePath(const char *str, ...) const;
    fs_path UOFilesPath(const char *str, ...) const;
    fs_path UOFilesPath(const std::string &str, ...) const;
};

}; // namespace Wisp

#endif // WISP_APP_H
