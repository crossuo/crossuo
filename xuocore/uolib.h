#pragma once

#include <common/utils.h>

extern void uo_log(int type, const char *sys, const char *fmt, ...);
extern void dump_hex(int type, const char *sys, const char *title, uint8_t *buf, int size);

#define PRINT_LOG(type, sys, ...) uo_log(type, sys, __VA_ARGS__)
#define TRACE(sys, ...) PRINT_LOG(1, MACRO_TOSTRING(sys), __VA_ARGS__)
#define DEBUG(sys, ...) PRINT_LOG(2, MACRO_TOSTRING(sys), __VA_ARGS__)
#define Info(sys, ...) PRINT_LOG(3, MACRO_TOSTRING(sys), __VA_ARGS__)
#define Warning(sys, ...) PRINT_LOG(4, MACRO_TOSTRING(sys), __VA_ARGS__)
#define Error(sys, ...) PRINT_LOG(5, MACRO_TOSTRING(sys), __VA_ARGS__)
#define DEBUG_DUMP(...) dump_hex(1, "Data", __VA_ARGS__)