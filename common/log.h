/*
The MIT License (MIT)
Copyright (c) 2017 Danny Angelo Carminati Grein
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef LOG_HEADER
#define LOG_HEADER

#ifndef LOG_TAG
#define LOG_TAG "log"
#endif

#ifndef LOG_ID_TRACE
#define LOG_ID_TRACE 1
#endif

#ifndef LOG_ID_DEBUG
#define LOG_ID_DEBUG 2
#endif

#ifndef LOG_ID_INFO
#define LOG_ID_INFO 3
#endif

#ifndef LOG_ID_WARN
#define LOG_ID_WARN 4
#endif

#ifndef LOG_ID_ERROR
#define LOG_ID_ERROR 5
#endif

#ifndef LOG_TRACE
#define LOG_TRACE(...) log_impl(LOG_ID_TRACE, LOG_TAG, __VA_ARGS__)
#endif

#ifndef LOG_DEBUG
#define LOG_DEBUG(...) log_impl(LOG_ID_DEBUG, LOG_TAG, __VA_ARGS__)
#endif

#ifndef LOG_INFO
#define LOG_INFO(...) log_impl(LOG_ID_INFO, LOG_TAG, __VA_ARGS__)
#endif

#ifndef LOG_WARN
#define LOG_WARN(...) log_impl(LOG_ID_WARN, LOG_TAG, __VA_ARGS__)
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(...) log_impl(LOG_ID_ERROR, LOG_TAG, __VA_ARGS__)
#endif

#ifndef LOG_ONLY
#define LOG_ONLY(x) (void)x;
#endif

void log_impl(int type, const char *name, const char *fmt, ...);

#endif // LOG_HEADER

#if defined(LOG_IMPLEMENTATION) && !defined(LOG_IMPLEMENTATED)
#define LOG_IMPLEMENTATED

#include <stdio.h>
#include <stdarg.h>

void log_impl(int type, const char *name, const char *fmt...)
{
    auto out = stdout;
    const char *tag = " INFO";

    switch (type)
    {
        case LOG_ID_TRACE:
            tag = "TRACE";
            break;
        case LOG_ID_DEBUG:
            tag = "DEBUG";
            break;
        case LOG_ID_INFO:
            break;
        case LOG_ID_WARN:
            tag = " WARN";
            out = stderr;
            break;
        case LOG_ID_ERROR:
            tag = "ERROR";
            out = stderr;
            break;
    }
    fprintf(out, "%s:%s: ", name, tag);
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
#ifdef LOG_NEWLINE
    fprintf(out, "\n");
#endif
}

#endif //  LOG_IMPLEMENTATION
