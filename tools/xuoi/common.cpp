// MIT License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#define CHECKSUM_IMPLEMENTATION
#include <common/checksum.h>

#define FS_IMPLEMENTATION
#include <common/fs.h>

#define LOOKUP3_IMPLEMENTATION
#include <external/lookup3.h>

bool valid_url(const astr_t &url)
{
    return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
}

#if defined(XUO_WINDOWS)
#include <windows.h>
#include <shellapi.h>

void open_url(const astr_t &url)
{
    assert(valid_url(url) && "invalid url format");
    ShellExecuteA(0, "Open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
#else
void open_url(const astr_t &url)
{
#if defined(XUO_OSX)
#define OPEN_CMD "open "
#else
#define OPEN_CMD "xdg-open "
#endif
    assert(valid_url(url) && "invalid url format");
    auto cmd = astr_t(OPEN_CMD) + url;
    system(cmd.c_str());
}
#endif
