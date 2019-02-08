// GPLv3
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include "Logging.h"

#if !defined(DISABLE_LOG)

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#define LOG_FORMAT "[%T.%f] [%n] [%^%l%$] [%s:%#] [%!] %v"

#if defined(XUO_WINDOWS)
#include <spdlog/sinks/msvc_sink.h>
using stdout_sink = spdlog::sinks::msvc_sink_mt;
#else
using stdout_sink = spdlog::sinks::stdout_color_sink_st;
#endif
using file_sink = spdlog::sinks::rotating_file_sink_mt;

eLogSystem g_LogEnabled = LogSystemAll;

static std::shared_ptr<file_sink> s_logFile;
static std::shared_ptr<stdout_sink> s_logOut;

#define LOG_SYSTEM(id, name) LOG_DECLARE_SYSTEM(name)
#include "Loggers.h"
#undef LOG_SYSTEM

void LogInit(const char *filename)
{
    s_logFile = std::make_shared<file_sink>(filename, 1024 * 1024 * 2, 5, true);
    s_logOut = std::make_shared<stdout_sink>();
    std::vector<spdlog::sink_ptr> sinks{ s_logFile, s_logOut };

#define LOG_SYSTEM(id, name) LOG_DEFINE_SYSTEM(name)
#include "Loggers.h"
#undef LOG_SYSTEM

    spdlog::set_default_logger(g_logClient);
    spdlog::set_pattern(LOG_FORMAT);
}

void LogHexBuffer(eLogSystem sys, int level, const char *title, uint8_t *buf, int size)
{
    auto logger = g_logClient;
    switch (sys)
    {
#define LOG_SYSTEM(id, name)                                                                       \
    case eLogSystem::LogSystem##name:                                                              \
        logger = g_log##name;                                                                      \
        break;
#include "Loggers.h"
#undef LOG_SYSTEM
        default:
            break;
    }

    auto lvl = (spdlog::level::level_enum)level;

    logger->log(lvl, title);
    int num_lines = size / 16;
    if (size % 16 != 0)
    {
        num_lines++;
    }

    for (int line = 0; line < num_lines; line++)
    {
        char out[128];
        char *cur = out;
        char *end = cur + sizeof(out);
        int row = 0;
        int r = 0;
        r = snprintf(cur, end - cur, "%04X: ", line * 16);
        assert(r >= 0 && r < end - cur && end - cur >= 0);
        cur += r;
        for (row = 0; row < 16; row++)
        {
            if (line * 16 + row < size)
            {
                r = snprintf(cur, end - cur, "%02X ", buf[line * 16 + row]);
                assert(r >= 0 && r < end - cur && end - cur >= 0);
                cur += r;
            }
            else
            {
                r = snprintf(cur, end - cur, "-- ");
                assert(r >= 0 && r < end - cur && end - cur >= 0);
                cur += r;
            }
        }
        r = snprintf(cur, end - cur, ": ");
        assert(r >= 0 && r < end - cur && end - cur >= 0);
        cur += r;
        for (row = 0; row < 16; row++)
        {
            if (line * 16 + row < size)
            {
                const char ch = isprint(buf[line * 16 + row]) != 0 ? buf[line * 16 + row] : '.';
                r = snprintf(cur, end - cur, "%c", ch);
                assert(r >= 0);
                assert(r < end - cur);
                assert(end - cur >= 0);
                cur += r;
            }
        }
        logger->log(lvl, out);
    }
}

#else

void LogInit(const char *filename)
{
}
void LogHexBuffer(const char *title, uint8_t *buf, int size)
{
}

#endif
