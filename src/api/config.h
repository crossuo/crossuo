#pragma once

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

#if LIBUO
#define TRACE(type, ...) fprintf(stderr, __VA_ARGS__)
#define DEBUG(type, ...) fprintf(stderr, __VA_ARGS__)
#define Info(type, ...) fprintf(stderr, __VA_ARGS__)
#define Warning(type, ...) fprintf(stderr, __VA_ARGS__)
#define Error(type, ...) fprintf(stderr, __VA_ARGS__)
#endif
