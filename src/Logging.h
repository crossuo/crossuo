// GPLv3
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

#undef ERROR

#if !defined(DISABLE_LOG)

#include <loguru.h>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define LOG_DECLARE_EXTERN(name) extern int g_log##name;
#define LOG_DECLARE_SYSTEM(name) int g_log##name;
#define LOG_DEFINE_SYSTEM(name) g_log##name = (int)eLogSystem::LogSystem##name;

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

#define LOG_(system, level, ...)                                                                   \
    VLOG_IF_F(                                                                                     \
        (int)eLogSystem::LogSystem##system,                                                        \
        loguru::Verbosity_##level,                                                                 \
        (g_LogEnabled & eLogSystem::LogSystem##system),                                            \
        __VA_ARGS__)

#define LOG_DUMP_(system, level, ...)                                                              \
    if ((g_LogEnabled & eLogSystem::LogSystem##system) &&                                          \
        loguru::current_verbosity_cutoff() > level)                                                \
    LogHexBuffer(eLogSystem::LogSystem##system, int(level), __VA_ARGS__)

#define Info(system, ...) LOG_(system, INFO, __VA_ARGS__)
#define Warning(system, ...) LOG_(system, WARNING, __VA_ARGS__)
#define Error(system, ...) LOG_(system, ERROR, __VA_ARGS__)
#define Fatal(system, ...) LOG_(system, FATAL, __VA_ARGS__)

#define INFO_DUMP(system, ...) LOG_DUMP_(system, 0, __VA_ARGS__)

#if defined(XUO_DEBUG)
#define TRACE(system, ...) LOG_(system, 9, __VA_ARGS__)
#define DEBUG(system, ...) LOG_(system, 1, __VA_ARGS__)
#define TRACE_DUMP(system, ...) LOG_DUMP_(system, 9, __VA_ARGS__)
#define DEBUG_DUMP(system, ...) LOG_DUMP_(system, 1, __VA_ARGS__)
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
#define Info(...)
#define Warning(...)
#define Error(...)
#define Fatal(...)
#define TRACE_DUMP(...)
#define DEBUG_DUMP(...)
#define INFO_DUMP(...)
#define SAFE_DEBUG_DUMP(...)

#endif

void LogHexBuffer(eLogSystem system, int level, const char *title, uint8_t *buf, int size);
void LogInit(int argc, char *argv[], const char *filename);
