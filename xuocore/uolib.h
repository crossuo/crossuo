#pragma once

#include <common/utils.h>

extern void uo_log(const char *type, ...);

#define PRINT_LOG(type, ...) uo_log(type, __VA_ARGS__)
#define TRACE(type, ...) PRINT_LOG(MACRO_TOSTRING(type), __VA_ARGS__)
#define DEBUG(type, ...) PRINT_LOG(MACRO_TOSTRING(type), __VA_ARGS__)
#define Info(type, ...) PRINT_LOG(MACRO_TOSTRING(type), __VA_ARGS__)
#define Warning(type, ...) PRINT_LOG(MACRO_TOSTRING(type), __VA_ARGS__)
#define Error(type, ...) PRINT_LOG(MACRO_TOSTRING(type), __VA_ARGS__)
