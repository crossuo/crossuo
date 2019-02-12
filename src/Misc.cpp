// MIT License

#include "Misc.h"
#include <locale>
#include <codecvt>

SDL_threadID g_MainThread;

#if USE_DEBUG_FUNCTION_NAMES == 1
std::deque<std::string> g_DebugFuncStack;
#endif

int CalculatePercents(int max, int current, int maxValue)
{
    if (max > 0)
    {
        max = (current * 100) / max;

        if (max > 100)
        {
            max = 100;
        }

        if (max > 1)
        {
            max = (maxValue * max) / 100;
        }
    }

    return max;
}

std::string EncodeUTF8(const std::wstring &wstr)
{
#if defined(XUO_WINDOWS)
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

std::wstring DecodeUTF8(const std::string &str)
{
#if defined(XUO_WINDOWS)
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
        Warning(Client, "DecodeUTF8 Failed: %s", str.c_str());
        INFO_DUMP(Client, "DecodeUTF8 Failed:", (uint8_t *)str.data(), str.size());
        return ToWString(str);
    }

    if (size > 0)
    {
        result.resize(size);
        mbsrtowcs(&result[0], &p, size, &state);
    }
#endif
    return result;
}

std::string ToCamelCaseA(std::string str)
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

std::wstring ToCamelCaseW(std::wstring str)
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

#if !defined(XUO_WINDOWS)
const std::string &ToString(const std::string &str)
{
    return str;
}
#endif

std::string ToString(const std::wstring &wstr)
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

std::wstring ToWString(const std::string &str)
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

std::string Trim(const std::string &str)
{
    string::const_iterator it = str.begin();
    for (; it != str.end() && (isspace(*it) != 0); ++it)
    {
        ;
    }

    string::const_reverse_iterator rit = str.rbegin();
    for (; rit.base() != it && (isspace(*rit) != 0); ++rit)
    {
        ;
    }

    return std::string(it, rit.base());
}

int ToInt(const std::string &str)
{
    return atoi(str.c_str());
}

std::string ToLowerA(std::string s)
{
#if defined(XUO_WINDOWS)
    if (s.length())
        _strlwr(&s[0]);

    return s;
#else
    std::transform(s.begin(), s.end(), s.begin(), [](auto c) { return std::tolower(c); });
    return s;
#endif
}

std::wstring ToLowerW(std::wstring s)
{
#if defined(XUO_WINDOWS)
    if (s.length())
        _wcslwr(&s[0]);

    return s;
#else
    std::transform(s.begin(), s.end(), s.begin(), [](auto c) { return std::towlower(c); });
    return s;
#endif
}

std::string ToUpperA(std::string s)
{
#if defined(XUO_WINDOWS)
    if (s.length())
        _strupr(&s[0]);

    return s;
#else
    std::transform(s.begin(), s.end(), s.begin(), [](auto c) { return std::toupper(c); });
    return s;
#endif
}

std::wstring ToUpperW(std::wstring s)
{
#if defined(XUO_WINDOWS)
    if (s.length())
        _wcsupr(&s[0]);

    return s;
#else
    std::transform(s.begin(), s.end(), s.begin(), [](auto c) { return std::towupper(c); });
    return s;
#endif
}

bool ToBool(const std::string &str)
{
    std::string data = ToLowerA(str);

    const int countOfTrue = 3;
    const string m_TrueValues[countOfTrue] = { "on", "yes", "true" };
    bool result = false;

    for (int i = 0; i < countOfTrue && !result; i++)
    {
        result = (data == m_TrueValues[i]);
    }

    return result;
}
