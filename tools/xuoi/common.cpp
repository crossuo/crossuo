// MIT License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#define LOG_NEWLINE
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

bool valid_url(const std::string &url)
{
    return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
}

#if defined(XUO_WINDOWS)
#include <windows.h>
#include <shellapi.h>

void open_url(const std::string &url)
{
    assert(valid_url(url) && "invalid url format");
    ShellExecuteA(0, "Open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
#else
void open_url(const std::string &url)
{
#if defined(XUO_OSX)
#define OPEN_CMD "open "
#else
#define OPEN_CMD "xdg-open "
#endif
    assert(valid_url(url) && "invalid url format");
    auto cmd = std::string(OPEN_CMD) + url;
    system(cmd.c_str());
}
#endif
