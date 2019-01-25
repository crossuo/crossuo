// MIT License
// Copyright (c) Hotride

#include "FileSystem.h"

CLogger g_Logger;
CLogger g_CrashLogger;

CLogger::CLogger()
{
}

CLogger::~CLogger()
{
    Close();
}

void CLogger::Close()
{
    if (m_File != nullptr)
    {
        LOG("Log closed.\n");
        fs_close(m_File);
        m_File = nullptr;
    }
}

void CLogger::Init(const os_path &filePath)
{
    if (m_File != nullptr)
    {
        fs_close(m_File);
    }

    m_File = fs_open(filePath, FS_WRITE);

    if (this == &g_Logger)
    {
        LOG("Log opened.\n");
    }

    FileName = filePath;
}

void CLogger::Print(const char *format, ...)
{
    if (m_File == nullptr)
    {
        return;
    }

    va_list arg;
    va_start(arg, format);
    vfprintf(m_File, format, arg);
    va_end(arg);
    fflush(m_File);
}

void CLogger::VPrint(const char *format, va_list ap)
{
    if (m_File == nullptr)
    {
        return;
    }

    vfprintf(m_File, format, ap);
    fflush(m_File);
}

void CLogger::Print(const wchar_t *format, ...)
{
    if (m_File == nullptr)
    {
        return;
    }

    va_list arg;
    va_start(arg, format);
    vfwprintf(m_File, format, arg);
    va_end(arg);
    fflush(m_File);
}

void CLogger::VPrint(const wchar_t *format, va_list ap)
{
    if (m_File == nullptr)
    {
        return;
    }

    vfwprintf(m_File, format, ap);
    fflush(m_File);
}

void CLogger::Dump(uint8_t *buf, int size)
{
    LogDump(m_File, buf, size);
}

void LogDump(FILE *fp, uint8_t *buf, int size)
{
    if (fp == nullptr)
    {
        return;
    }

    int num_lines = size / 16;

    if (size % 16 != 0)
    {
        num_lines++;
    }

    for (int line = 0; line < num_lines; line++)
    {
        int row = 0;
        fprintf(fp, "%04X: ", line * 16);

        for (row = 0; row < 16; row++)
        {
            if (line * 16 + row < size)
            {
                fprintf(fp, "%02X ", buf[line * 16 + row]);
            }
            else
            {
                fprintf(fp, "-- ");
            }
        }

        fprintf(fp, ": ");

        for (row = 0; row < 16; row++)
        {
            if (line * 16 + row < size)
            {
                fputc(isprint(buf[line * 16 + row]) != 0 ? buf[line * 16 + row] : '.', fp);
            }
        }

        fputc('\n', fp);
    }

    fflush(fp);
}
