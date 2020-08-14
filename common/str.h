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

#ifndef STR_HEADER
#define STR_HEADER

#include <external/miniconv.h>
#include <string.h>
#include <locale>
#include <codecvt>
#include <assert.h>

#include <string>
using astr_t = std::string;
using wstr_t = std::wstring;
struct utf8_t
{
    std::string _s;
};

#if defined(_MSC_VER)
#include <Windows.h>

#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif

#else

#include <algorithm>

#endif

// TODO: encapsulate wstr_t into a wstr_t type and astr_t into a str_t type
// so we can migrate for something lightweight in performance and memory

void str_init();
void str_terminate();
utf8_t utf8_from(const char *str, size_t size);
utf8_t utf8_from(astr_t aStr);
utf8_t utf8_from(wstr_t aWstr);

astr_t wstr_to_utf8(const wstr_t &aWstr);  // DEPRECATE, in memory everything should be UTF8
wstr_t wstr_from_utf8(const astr_t &aStr); // DEPRECATE
wstr_t wstr_from_utf8_input(const astr_t &aStr);
astr_t str_camel_case(astr_t aStr);
wstr_t wstr_camel_case(wstr_t aWstr);

astr_t str_from(const wstr_t &aWstr);
astr_t str_from(int val);
astr_t str_from(const utf8_t &str);
wstr_t wstr_from(const astr_t &aStr);
wstr_t wstr_from(int val);
wstr_t wstr_from(const utf8_t &str);
#if !defined(_MSC_VER)
const astr_t &str_from(const astr_t &aStr);
#endif

astr_t str_trim(const astr_t &aStr);
int str_to_int(const astr_t &aStr);
int str_to_int(const wstr_t &aWstr);
const char *str_lower(const char *aCStr);
astr_t str_lower(astr_t aStr);
astr_t str_upper(astr_t aStr);
wstr_t wstr_lower(wstr_t aWstr);
wstr_t wstr_upper(wstr_t aWstr);
bool str_to_bool(const astr_t &aStr);

#endif // STR_HEADER

#if defined(STR_IMPLEMENTATION) && !defined(STR_IMPLEMENTATED)
#define STR_IMPLEMENTATED

static miniconv_t s_utf8_utf8;   // cleanup invalid data
static miniconv_t s_cp1250_utf8; // Issue #221
static miniconv_t *s_encodings[] = { &s_utf8_utf8, &s_cp1250_utf8 };

void str_init()
{
    s_utf8_utf8 = miniconv_open("utf-8//IGNORE", "utf-8");
    s_cp1250_utf8 = miniconv_open("utf-8//IGNORE", "cp1250");
}

void str_terminate()
{
    constexpr auto count = sizeof(s_encodings) / sizeof(s_encodings[0]);
    for (int e = 0; e < count; e++)
    {
        miniconv_close(*s_encodings[e]);
    }
}

utf8_t utf8_from(const char *str, size_t size)
{
    utf8_t s;
    char buf[512];
    constexpr auto count = sizeof(s_encodings) / sizeof(s_encodings[0]);
    for (int e = 0; e < count; e++)
    {
        const char *in_buf = (const char *)str;
        size_t in_size = size;
        char *out_buf = &buf[0];
        size_t out_size = sizeof(buf);
        int err = 0;
        miniconv2(*s_encodings[e], &in_buf, &in_size, &out_buf, &out_size, &err);
        if (err == 0)
        {
            out_size = sizeof(buf) - out_size;
            s._s = std::string(reinterpret_cast<char const *>(buf), out_size);
            return s;
        }
    }
    s._s = "<decode error>";
    return s;
}

utf8_t utf8_from(astr_t aStr)
{
    return utf8_from(aStr.data(), aStr.size());
}

utf8_t utf8_from(wstr_t aWstr)
{
    return utf8_from((char *)aWstr.data(), aWstr.size());
}

const char *str_lower(const char *aCStr)
{
    char *s = const_cast<char *>(aCStr);
    assert(s);
    while (*s)
    {
        *s = tolower(*s);
        s++;
    }
    return aCStr;
}

astr_t wstr_to_utf8(const wstr_t &aWstr)
{
#if defined(_MSC_VER)
    int size = ::WideCharToMultiByte(
        CP_UTF8, 0, &aWstr[0], (int)aWstr.size(), nullptr, 0, nullptr, nullptr);
    astr_t result = "";

    if (size > 0)
    {
        result.resize(size + 1);
        ::WideCharToMultiByte(
            CP_UTF8, 0, &aWstr[0], (int)aWstr.size(), &result[0], size, nullptr, nullptr);
        result.resize(size); // result[size] = 0;
    }
#else
    mbstate_t state{};
    astr_t result{};
    auto p = aWstr.data();
    const auto size = wcsrtombs(nullptr, &p, 0, &state);
    if (size > 0)
    {
        result.resize(size + 1);
        wcsrtombs(&result[0], &p, size, &state);
        result.resize(size);
    }
#endif

    return result;
}

wstr_t wstr_from_utf8(const astr_t &aStr)
{
#if defined(_MSC_VER)
    int size = ::MultiByteToWideChar(CP_UTF8, 0, &aStr[0], (int)aStr.size(), nullptr, 0);
    wstr_t result = {};
    if (size > 0)
    {
        result.resize(size + 1);
        ::MultiByteToWideChar(CP_UTF8, 0, &aStr[0], (int)aStr.size(), &result[0], size);
        result.resize(size); // result[size] = 0;
    }
#else
    mbstate_t state{};
    wstr_t result{};
    auto p = aStr.data();

    const size_t size = mbsrtowcs(nullptr, &p, 0, &state);
    if (size == -1)
    {
        Warning(Client, "wstr_from_utf8 Failed: %s", aStr.c_str());
        INFO_DUMP(Client, "wstr_from_utf8 Failed:", (uint8_t *)aStr.data(), aStr.size());
        return wstr_from(aStr);
    }

    if (size > 0)
    {
        result.resize(size);
        mbsrtowcs(&result[0], &p, size, &state);
    }
#endif

    return result;
}

wstr_t wstr_from_utf8_input(const astr_t &aStr)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> convert;
    const std::wstring result = convert.from_bytes(aStr);
    return result;
}

astr_t str_from(const utf8_t &str)
{
    return str._s;
}

astr_t str_from(const wstr_t &aWstr)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.to_bytes(aWstr);
}

wstr_t wstr_from(const astr_t &aStr)
{
    utf8_t s = utf8_from(aStr);
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(s._s);
}

wstr_t wstr_from(const utf8_t &str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str._s);
}

astr_t str_camel_case(astr_t aStr)
{
    int offset = 'a' - 'A';
    bool lastSpace = true;

    for (char &c : aStr)
    {
        if (c == ' ')
        {
            lastSpace = true;
        }
        else if (lastSpace)
        {
            lastSpace = false;

            if (c >= 'a' && c <= 'z')
            {
                c -= offset;
            }
        }
    }

    return aStr;
}

wstr_t wstr_camel_case(wstr_t aWstr)
{
    int offset = L'a' - L'A';
    bool lastSpace = true;

    for (wchar_t &c : aWstr)
    {
        if (c == L' ')
        {
            lastSpace = true;
        }
        else if (lastSpace)
        {
            lastSpace = false;

            if (c >= L'a' && c <= L'z')
            {
                c -= offset;
            }
        }
    }

    return aWstr;
}

#if !defined(_MSC_VER)
const astr_t &str_from(const astr_t &str)
{
    return str;
}
#endif

astr_t str_trim(const astr_t &aStr)
{
    auto it = aStr.begin();
    for (; it != aStr.end() && (isspace(*it) != 0); ++it)
    {
        ;
    }

    auto rit = aStr.rbegin();
    for (; rit.base() != it && (isspace(*rit) != 0); ++rit)
    {
        ;
    }

    return astr_t(it, rit.base());
}

astr_t str_lower(astr_t aStr)
{
#if defined(_MSC_VER)
    if (aStr.length())
        _strlwr(&aStr[0]);

    return aStr;
#else
    std::transform(aStr.begin(), aStr.end(), aStr.begin(), [](auto c) { return std::tolower(c); });
    return aStr;
#endif
}

wstr_t wstr_lower(wstr_t aWstr)
{
#if defined(_MSC_VER)
    if (aWstr.length())
        _wcslwr(&aWstr[0]);

    return aWstr;
#else
    std::transform(
        aWstr.begin(), aWstr.end(), aWstr.begin(), [](auto c) { return std::towlower(c); });
    return aWstr;
#endif
}

astr_t str_upper(astr_t aStr)
{
#if defined(_MSC_VER)
    if (aStr.length())
        _strupr(&aStr[0]);

    return aStr;
#else
    std::transform(aStr.begin(), aStr.end(), aStr.begin(), [](auto c) { return std::toupper(c); });
    return aStr;
#endif
}

wstr_t wstr_upper(wstr_t aWstr)
{
#if defined(_MSC_VER)
    if (aWstr.length())
        _wcsupr(&aWstr[0]);

    return aWstr;
#else
    std::transform(
        aWstr.begin(), aWstr.end(), aWstr.begin(), [](auto c) { return std::towupper(c); });
    return aWstr;
#endif
}

bool str_to_bool(const astr_t &aStr)
{
    astr_t data = str_lower(aStr);
    const int countOfTrue = 3;
    const astr_t m_TrueValues[countOfTrue] = { "on", "yes", "true" };
    bool result = false;
    for (int i = 0; i < countOfTrue && !result; i++)
    {
        result = (data == m_TrueValues[i]);
    }
    return result;
}

int str_to_int(const astr_t &aStr)
{
    return std::stoi(aStr);
}

int str_to_int(const wstr_t &aWstr)
{
    return std::stoi(aWstr);
}

astr_t str_from(int64_t val)
{
    return std::to_string(val);
}

wstr_t wstr_from(int64_t val)
{
    return std::to_wstring(val);
}

astr_t str_from(uint64_t val)
{
    return std::to_string(val);
}

wstr_t wstr_from(uint64_t val)
{
    return std::to_wstring(val);
}

astr_t str_from(int val)
{
    return std::to_string(val);
}

wstr_t wstr_from(int val)
{
    return std::to_wstring(val);
}
#endif // STR_IMPLEMENTATION
