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

namespace xuoassist {

uint TextToUInt(const std::string &text);
uint TextToSerial(std::string text);
ushort TextToGraphic(std::string text);
std::string SerialToText(const uint &value);
std::string GraphicToText(const ushort &value);

// The FIND_TYPE_FLAGS constants now live in core/global.h (moved here from
// this file by the assist-core round, which owns them in the original app).

} // namespace xuoassist

