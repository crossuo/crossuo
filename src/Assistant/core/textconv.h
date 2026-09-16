// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Temporary home for the CAssistant static text-conversion helpers that the
// model layer (CFindItem/CGameObject) needs (they live in assist/assist.* in
// the original app). Bodies are the originals, trimmed; the CAssistant static
// methods and the config-module list lookups (SeekObject/SeekType) both
// resolve to these.

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include <common/str.h> // astr_t

namespace xuoassist {

uint32_t TextToUInt(const astr_t &text);
uint32_t TextToSerial(astr_t text);
uint16_t TextToGraphic(astr_t text);
astr_t SerialToText(const uint32_t &value);
astr_t GraphicToText(const uint16_t &value);

// Tokenizes one line of text with the X:UO Assist text-parser semantics
// (delimiters and comment markers are single-character sets; `quotes` is a
// list of (openQuote, closeQuote) pairs; `trim` strips spaces/tabs from
// unquoted tokens; `saveQuotes` keeps the quote characters on quoted
// fragments). xuocore's TextFileParser is file-based and its unquoted-token
// scan does not stop at quote characters, so the quote-aware call sites
// (the display-tag compiler and the text-command dispatcher) keep this
// line-scoped helper instead. `line` must not contain '\n'; empty tokens
// are dropped.
std::vector<astr_t> ParseTextTokens(
    const astr_t &line,
    const char *delimiters,
    const char *comentaries,
    const char *quotes,
    bool trim = true,
    bool saveQuotes = false);

// The FIND_TYPE_FLAGS constants now live in core/global.h (moved here from
// this file by the assist-core round, which owns them in the original app).

} // namespace xuoassist
