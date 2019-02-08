// GPLv3
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

#if !defined(DISABLE_LOG)

#include <spdlog/spdlog.h>
#include <spdlog/async.h>

using logger = spdlog::logger;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define LOG_DECLARE_EXTERN(name) extern std::shared_ptr<logger> g_log##name;
#define LOG_DECLARE_SYSTEM(name) std::shared_ptr<logger> g_log##name;
#define LOG_DEFINE_SYSTEM(name)                                                                    \
    g_log##name = std::make_shared<spdlog::logger>(TOSTRING(name), begin(sinks), end(sinks));

using LogSystem = uint32_t;

enum eLogSystem : LogSystem
{
#define LOG_SYSTEM(id, name) LogSystem##name = 1 << id,
#include "Loggers.h"
#undef LOG_SYSTEM
    LogSystemAll = ~0u,
};

extern eLogSystem g_LogEnabled;

#define LOG_SYSTEM(id, name) LOG_DECLARE_EXTERN(name)
#include "Loggers.h"
#undef LOG_SYSTEM

#define LOG_(system, logger, level, ...)                                                           \
    if ((g_LogEnabled & system) && logger->should_log(level))                                      \
    logger->log(                                                                                   \
        spdlog::source_loc{ SPDLOG_FILE_BASENAME(__FILE__), __LINE__, SPDLOG_FUNCTION },           \
        level,                                                                                     \
        __VA_ARGS__)

#define LOG_DUMP_(system, level, ...)                                                              \
    if ((g_LogEnabled & eLogSystem::LogSystem##system) && g_log##system->should_log(level))        \
    LogHexBuffer(eLogSystem::LogSystem##system, int(level), __VA_ARGS__)

#define INFO(system, ...)                                                                          \
    LOG_(eLogSystem::LogSystem##system, g_log##system, spdlog::level::info, __VA_ARGS__)
#define WARN(system, ...)                                                                          \
    LOG_(eLogSystem::LogSystem##system, g_log##system, spdlog::level::warn, __VA_ARGS__)
#define ERROR(system, ...)                                                                         \
    LOG_(eLogSystem::LogSystem##system, g_log##system, spdlog::level::err, __VA_ARGS__)
#define CRITICAL(system, ...)                                                                      \
    LOG_(eLogSystem::LogSystem##system, g_log##system, spdlog::level::critical, __VA_ARGS__)

#define INFO_DUMP(system, ...) LOG_DUMP_(system, spdlog::level::info, __VA_ARGS__)

#if defined(XUO_DEBUG)
#define TRACE(system, ...)                                                                         \
    LOG_(eLogSystem::LogSystem##system, g_log##system, spdlog::level::trace, __VA_ARGS__)
#define DEBUG(system, ...)                                                                         \
    LOG_(eLogSystem::LogSystem##system, g_log##system, spdlog::level::debug, __VA_ARGS__)
#define TRACE_DUMP(system, ...) LOG_DUMP_(system, spdlog::level::trace, __VA_ARGS__)
#define DEBUG_DUMP(system, ...) LOG_DUMP_(system, spdlog::level::debug, __VA_ARGS__)
#define SAFE_DEBUG_DUMP(system, ...) DEBUG_DUMP(system, __VA_ARGS__)
#else
#define TRACE(...)
#define DEBUG(...)
#define TRACE_DUMP(...)
#define DEBUG_DUMP(...)
#define SAFE_DEBUG_DUMP(...)
#endif

#else

#define TRACE(...)
#define DEBUG(...)
#define INFO(...)
#define WARN(...)
#define ERROR(...)
#define CRITICAL(...)
#define TRACE_DUMP(...)
#define DEBUG_DUMP(...)
#define INFO_DUMP(...)
#define SAFE_DEBUG_DUMP(...)

#endif

void LogHexBuffer(eLogSystem system, int level, const char *title, uint8_t *buf, int size);
void LogInit(const char *filename);
