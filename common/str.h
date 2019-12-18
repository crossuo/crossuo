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

#include <string.h>
#include <locale>
#include <codecvt>
#include <assert.h>

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

// TODO: encapsulate std::wstring into a wstr_t type and std::string into a str_t type
// so we can migrate for something lightweight in performance and memory

std::string wstr_to_utf8(const std::wstring &str); // DEPRECATE, in memory everything should be UTF8
std::wstring wstr_from_utf8(const std::string &str); // DEPRECATE
std::string str_camel_case(std::string str);
std::wstring wstr_camel_case(std::wstring str);

std::string str_from(const std::wstring &wstr);
std::wstring wstr_from(const std::string &str);
#if !defined(_MSC_VER)
const std::string &str_from(const std::string &str);
#endif

std::string str_trim(const std::string &str);
int str_to_int(const std::string &str);
const char *str_lower(const char *str);
std::string str_lower(std::string str);
std::string str_upper(std::string str);
std::wstring wstr_lower(std::wstring str);
std::wstring wstr_upper(std::wstring str);
bool str_to_bool(const std::string &str);

#endif // STR_HEADER

#if defined(STR_IMPLEMENTATION) && !defined(STR_IMPLEMENTATED)
#define STR_IMPLEMENTATED

const char *str_lower(const char *str)
{
    char *s = const_cast<char *>(str);
    assert(s);
    while (*s)
    {
        *s = tolower(*s);
        s++;
    }
    return str;
}

std::string wstr_to_utf8(const std::wstring &wstr)
{
#if defined(_MSC_VER)
    int size =
        ::WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string result = "";

    if (size > 0)
    {
        result.resize(size + 1);
        ::WideCharToMultiByte(
            CP_UTF8, 0, &wstr[0], (int)wstr.size(), &result[0], size, nullptr, nullptr);
        result.resize(size); // result[size] = 0;
    }
#else
    mbstate_t state{};
    std::string result{};
    auto p = wstr.data();
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

std::wstring wstr_from_utf8(const std::string &str)
{
#if defined(_MSC_VER)
    int size = ::MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
    std::wstring result = {};
    if (size > 0)
    {
        result.resize(size + 1);
        ::MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], size);
        result.resize(size); // result[size] = 0;
    }
#else
    mbstate_t state{};
    std::wstring result{};
    auto p = str.data();

    const size_t size = mbsrtowcs(nullptr, &p, 0, &state);
    if (size == -1)
    {
        Warning(Client, "wstr_from_utf8 Failed: %s", str.c_str());
        INFO_DUMP(Client, "wstr_from_utf8 Failed:", (uint8_t *)str.data(), str.size());
        return wstr_from(str);
    }

    if (size > 0)
    {
        result.resize(size);
        mbsrtowcs(&result[0], &p, size, &state);
    }
#endif

    return result;
}

std::string str_camel_case(std::string str)
{
    int offset = 'a' - 'A';
    bool lastSpace = true;

    for (char &c : str)
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

    return str;
}

std::wstring wstr_camel_case(std::wstring str)
{
    int offset = L'a' - L'A';
    bool lastSpace = true;

    for (wchar_t &c : str)
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

    return str;
}

#if !defined(_MSC_VER)
const std::string &str_from(const std::string &str)
{
    return str;
}
#endif

std::string str_from(const std::wstring &wstr)
{
#if 0
    std::string str = "";
    int size = (int)wstr.length();
    int newSize =
        ::WideCharToMultiByte(GetACP(), 0, wstr.c_str(), size, nullptr, 0, nullptr, nullptr);

    if (newSize > 0)
    {
        str.resize(newSize + 1);
        ::WideCharToMultiByte(GetACP(), 0, wstr.c_str(), size, &str[0], newSize, nullptr, nullptr);
        str.resize(newSize); // str[newSize] = 0;
    }
    return str;
#else
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.to_bytes(wstr);
#endif
}

std::wstring wstr_from(const std::string &str)
{
#if 0
    int size = (int)str.length();
    std::wstring wstr = {};

    if (size > 0)
    {
        wstr.resize(size + 1);
        MultiByteToWideChar(GetACP(), 0, str.c_str(), size, &wstr[0], size);
        wstr.resize(size); // wstr[size] = 0;
    }
    return wstr;
#else
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str);
#endif
}

std::string str_trim(const std::string &str)
{
    auto it = str.begin();
    for (; it != str.end() && (isspace(*it) != 0); ++it)
    {
        ;
    }

    auto rit = str.rbegin();
    for (; rit.base() != it && (isspace(*rit) != 0); ++rit)
    {
        ;
    }

    return std::string(it, rit.base());
}

std::string str_lower(std::string s)
{
#if defined(_MSC_VER)
    if (s.length())
        _strlwr(&s[0]);

    return s;
#else
    std::transform(s.begin(), s.end(), s.begin(), [](auto c) { return std::tolower(c); });
    return s;
#endif
}

std::wstring wstr_lower(std::wstring s)
{
#if defined(_MSC_VER)
    if (s.length())
        _wcslwr(&s[0]);

    return s;
#else
    std::transform(s.begin(), s.end(), s.begin(), [](auto c) { return std::towlower(c); });
    return s;
#endif
}

std::string str_upper(std::string s)
{
#if defined(_MSC_VER)
    if (s.length())
        _strupr(&s[0]);

    return s;
#else
    std::transform(s.begin(), s.end(), s.begin(), [](auto c) { return std::toupper(c); });
    return s;
#endif
}

std::wstring wstr_upper(std::wstring s)
{
#if defined(_MSC_VER)
    if (s.length())
        _wcsupr(&s[0]);

    return s;
#else
    std::transform(s.begin(), s.end(), s.begin(), [](auto c) { return std::towupper(c); });
    return s;
#endif
}

bool str_to_bool(const std::string &str)
{
    std::string data = str_lower(str);
    const int countOfTrue = 3;
    const std::string m_TrueValues[countOfTrue] = { "on", "yes", "true" };
    bool result = false;
    for (int i = 0; i < countOfTrue && !result; i++)
    {
        result = (data == m_TrueValues[i]);
    }
    return result;
}

int str_to_int(const std::string &str)
{
    return atoi(str.c_str());
}

#endif // STR_IMPLEMENTATION