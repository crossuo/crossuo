// MIT License
// Copyright (c) Hotride

#pragma once

#include "FileSystem.h"

void LogDump(FILE *fp, uint8_t *buf, int size);

#define LOG_LEVEL 2

#if LOG_LEVEL == 1
#define INITLOGGER(path)
#define LOG(...) fprintf(stdout, " LOG: " __VA_ARGS__)
#define LOG_DUMP(...)      //LogDump(stdout, __VA_ARGS__)
#define SAFE_LOG_DUMP(...) //LogDump(stdout, __VA_ARGS__)
#elif LOG_LEVEL == 2
#define INITLOGGER(path) g_Logger.Init(path);
#define LOG(...) g_Logger.Print(__VA_ARGS__) //fprintf(stdout, " LOG: " __VA_ARGS__)
#define LOG_DUMP(...)                        //LogDump(stdout, __VA_ARGS__)
#define SAFE_LOG_DUMP(...)                   //LogDump(stdout, __VA_ARGS__)
#else                                        //LOG_LEVEL == 0
#define INITLOGGER(path)
#define LOG(...)
#define LOG_DUMP(...)
#define SAFE_LOG_DUMP(...)
#endif //LOG_LEVEL!=0

#define INITCRASHLOGGER(path) g_CrashLogger.Init(path);
#define CRASHLOG g_CrashLogger.Print
#define CRASHLOG_DUMP g_CrashLogger.Dump

class CLogger
{
public:
    os_path FileName;

protected:
    FILE *m_File{ nullptr };

public:
    CLogger();
    ~CLogger();

    void Close();
    bool Ready() const { return m_File != nullptr; }
    void Init(const os_path &filePath);
    void Print(const char *format, ...);
    void VPrint(const char *format, va_list ap);
    void Print(const wchar_t *format, ...);
    void VPrint(const wchar_t *format, va_list ap);
    void Dump(uint8_t *buf, int size);
};

extern CLogger g_Logger;
extern CLogger g_CrashLogger;
