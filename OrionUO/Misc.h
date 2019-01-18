// MIT License

#pragma once
/*
#if defined(ORION_WINDOWS)
#if defined(_MSC_VER)
#pragma warning(disable : 4800) //forcing value to bool 'true' or 'false' (performance warning)
#endif
#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <windows.h>
#endif

#include <SDL_thread.h>
#include <thread>

#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>

#include <string>
using std::string;
using std::wstring;

#include <vector>
using std::vector;

#include <deque>
using std::deque;

#include <map>
using std::map;

#include <unordered_map>
using std::pair;
using std::unordered_map;

#include "../Definitions.h"
*/
extern SDL_threadID g_MainThread;
const int PACKET_VARIABLE_SIZE = 0;

int CalculatePercents(int max, int current, int maxValue);

string EncodeUTF8(const wstring &str);
wstring DecodeUTF8(const string &str);
string ToCamelCaseA(string str);
wstring ToCamelCaseW(wstring str);
#if !defined(ORION_WINDOWS)
const string &ToString(const string &str);
#endif
string ToString(const wstring &wstr);
wstring ToWString(const string &str);
string Trim(const string &str);
int ToInt(const string &str);
string ToLowerA(string str);
string ToUpperA(string str);
wstring ToLowerW(wstring str);
wstring ToUpperW(wstring str);
bool ToBool(const string &str);

inline float deg2radf(float degr)
{
    return degr * (float)(M_PI / 180.0f);
}

inline float rad2degf(float radians)
{
    return (float)(radians * 180.0f / M_PI);
}

inline int RandomInt(int n)
{
    return (rand() % n);
}

inline int RandomIntMinMax(int n, int m)
{
    return (rand() % (m - n) + n);
}

inline uint32_t unpack32(uint8_t *buf)
{
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

inline uint16_t unpack16(uint8_t *buf)
{
    return (buf[0] << 8) | buf[1];
}

inline void pack32(uint8_t *buf, uint32_t x)
{
    buf[0] = uint8_t(x >> 24);
    buf[1] = uint8_t((x >> 16) & 0xff);
    buf[2] = uint8_t((x >> 8) & 0xff);
    buf[3] = uint8_t(x & 0xff);
}

inline void pack16(uint8_t *buf, uint16_t x)
{
    buf[0] = x >> 8;
    buf[1] = x & 0xff;
}

#if USE_DEBUG_FUNCTION_NAMES == 1
extern deque<string> g_DebugFuncStack;
class CFuncDebug
{
public:
    CFuncDebug::CFuncDebug(const char *str)
    {
        if (g_MainThread == Wisp::CThread::GetCurrentThreadId())
        {
            g_DebugFuncStack.push_back(str);
        }
    }
    CFuncDebug::~CFuncDebug()
    {
        if (g_MainThread == Wisp::CThread::GetCurrentThreadId())
        {
            g_DebugFuncStack.pop_back();
        }
    }
};
#endif
