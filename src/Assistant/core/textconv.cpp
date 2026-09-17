// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (assist/assist.cpp static helpers). See textconv.h.

#include "textconv.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "global.h"
#include "../config/assistantconfig.h"

namespace xuoassist {

// The harness (xuoassist-script-test) links this TU for ParseTextTokens only;
// the converters below consult the live client world (g_PlayerSerial, the
// g_Last*Object slots), and the harness provides stubs in harnessstubs.cpp.
#ifndef XUO_ASSISTANT_HARNESS

uint32_t TextToUInt(const astr_t &text)
{
    uint32_t result = (uint32_t)strtoul(text.c_str(), nullptr, 10);

    if (result == 0u)
    {
        // original: text.toInt() first, then toUInt(nullptr, 16) on failure
        const char *str = text.c_str();
        bool decimal = (text.length() != 0);
        for (size_t i = 0; i < text.length() && decimal; i++)
        {
            if (str[i] < '0' || str[i] > '9')
            {
                decimal = false;
            }
        }

        if (!decimal)
        {
            result = (uint32_t)strtoul(str, nullptr, 16);
        }
    }

    return result;
}

uint32_t TextToSerial(astr_t text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }

    uint32_t result = 0;

    if (text == "self")
    {
        result = g_PlayerSerial;
    }
    else if (text == "lasttarget")
    {
        result = g_LastTargetObject;
    }
    else if (text == "lastattack")
    {
        result = g_LastAttackObject;
    }
    else if (text == "lastcorpse")
    {
        result = g_LastCorpseObject;
    }
    else if (text == "lastcontainer")
    {
        result = g_LastContainerObject;
    }
    else if (text == "laststatus")
    {
        result = g_LastStatusObject;
    }
    else if (text == "lastobject")
    {
        result = g_LastUseObject;
    }
    else if (text == "backpack")
    {
        result = g_Backpack;
    }
    else if (text == "ground")
    {
        result = 0xFFFFFFFF;
    }
    else
    {
        result = g_AssistantConfig.SeekObject(text);
    }

    if (result == 0u)
    {
        result = TextToUInt(text);
    }

    return result;
}

uint16_t TextToGraphic(astr_t text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }

    uint16_t result = g_AssistantConfig.SeekType(text);

    if (result == 0u)
    {
        result = (uint16_t)TextToUInt(text);
    }

    return result;
}

astr_t SerialToText(const uint32_t &value)
{
    char buf[64] = { 0 };
    snprintf(buf, sizeof(buf), "0x%08X", value);

    return buf;
}

astr_t GraphicToText(const uint16_t &value)
{
    char buf[64] = { 0 };
    snprintf(buf, sizeof(buf), "0x%04X", value);

    return buf;
}

#endif // !XUO_ASSISTANT_HARNESS

vector<astr_t> ParseTextTokens(
    const astr_t &line, const char *delimiters, const char *comentaries, const char *quotes,
    bool trim, bool saveQuotes)
{
    vector<astr_t> result;

    const size_t comentsSize = strlen(comentaries);
    const size_t quotesSize = strlen(quotes);
    const char *const data = line.data();
    // The old CTextParser bounded every line at the first embedded NUL
    // (GetEOL), so a NUL terminated the scan instead of stalling on it.
    // This matters for the display-tag flow: CompileData re-parses the
    // stage-one tokens, and quoted tokens carry a trailing NUL (the
    // CTextParser quirk), so "{name\0}" reaches this tokenizer verbatim.
    const char *const nul = (const char *)memchr(data, '\0', line.size());
    const char *const end = (nul != nullptr) ? nul : data + line.size();
    const char *ptr = data;

    const auto isDelimiter = [&](char c)
    {
        return c != '\0' && strchr(delimiters, c) != nullptr;
    };

    // A doubled marker ("//") comments only when it appears twice; a lone
    // marker char still comments once the doubled pair is ruled out.
    const auto isComment = [&](const char *at)
    {
        bool matched = false;

        for (size_t i = 0; i < comentsSize && !matched; i++)
        {
            matched = (*at == comentaries[i]);

            if (matched && i + 1 < comentsSize && comentaries[i] == comentaries[i + 1] &&
                at + 1 < end)
            {
                matched = (at[0] == at[1]);
                i++;
            }
        }

        return matched;
    };

    const auto isQuoteChar = [&](char c)
    {
        for (size_t i = 0; i < quotesSize; i++)
        {
            if (c == quotes[i])
            {
                return true;
            }
        }

        return false;
    };

    const auto isSecondQuote = [&](const char *at)
    {
        for (size_t i = 0; i < quotesSize; i += 2)
        {
            if (*at == quotes[i + 1])
            {
                return true;
            }
        }

        return false;
    };

    const auto obtainData = [&]()
    {
        astr_t out;

        while (ptr < end && *ptr != '\0')
        {
            if (isDelimiter(*ptr) || isQuoteChar(*ptr))
            {
                break;
            }

            if (isComment(ptr))
            {
                ptr = end;
                break;
            }

            if (*ptr != '\r' && (!trim || (*ptr != ' ' && *ptr != '\t')))
            {
                out += *ptr;
            }

            ptr++;
        }

        return out;
    };

    const auto obtainQuoted = [&]()
    {
        bool quoted = false;
        astr_t out;

        for (size_t i = 0; i < quotesSize; i += 2)
        {
            if (*ptr == quotes[i])
            {
                const char endQuote = quotes[i + 1];
                quoted = true;
                ptr++;
                const char *scan = ptr;

                while (scan < end && *scan != '\0' && *scan != endQuote)
                {
                    scan++;
                }

                const size_t size = (size_t)(scan - ptr);

                if (size > 0)
                {
                    // The tokenizer allocates one extra byte and stores the
                    // terminator inside the token, so quoted tokens keep a
                    // trailing NUL (the '\r' strip nulls in place too).
                    out.resize(size + 1);
                    memcpy(&out[0], ptr, size);
                    out[size] = '\0';

                    for (size_t j = size; j-- > 0 && out[j] == '\r';)
                    {
                        out[j] = '\0';
                    }

                    ptr = scan;

                    if (ptr < end && *ptr == endQuote)
                    {
                        ptr++;
                    }
                }

                if (saveQuotes)
                {
                    out.insert(out.begin(), quotes[i]);
                    out = astr_t(out) + endQuote;
                }

                break;
            }
        }

        if (!quoted)
        {
            out = obtainData();
        }

        return out;
    };

    while (ptr < end)
    {
        const char *const scanStart = ptr;

        while (ptr < end && *ptr != '\0' && isDelimiter(*ptr))
        {
            ptr++;
        }

        if (isComment(ptr))
        {
            break;
        }

        const astr_t buf = obtainQuoted();

        if (!buf.empty())
        {
            result.push_back(buf);
        }
        else if (isSecondQuote(ptr))
        {
            ptr++;
        }

        // Progress guarantee: no input may leave the scan position unchanged
        // (the only char every scan helper skips without consuming is a NUL,
        // which cannot appear before `end` anymore, but keep the backstop).
        if (ptr == scanStart)
        {
            ptr++;
        }
    }

    return result;
}

} // namespace xuoassist
