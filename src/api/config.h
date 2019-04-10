#pragma once

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

extern void uo_log(const char *type, ...);

#if LIBUO
#define LIBUO_STRINGIFY(x) #x
#define LIBUO_TOSTRING(x) LIBUO_STRINGIFY(x)

#define PRINT_LOG(type, ...) uo_log(type, __VA_ARGS__)
#define TRACE(type, ...) PRINT_LOG(LIBUO_TOSTRING(type), __VA_ARGS__)
#define DEBUG(type, ...) PRINT_LOG(LIBUO_TOSTRING(type), __VA_ARGS__)
#define Info(type, ...) PRINT_LOG(LIBUO_TOSTRING(type), __VA_ARGS__)
#define Warning(type, ...) PRINT_LOG(LIBUO_TOSTRING(type), __VA_ARGS__)
#define Error(type, ...) PRINT_LOG(LIBUO_TOSTRING(type), __VA_ARGS__)
#endif
