// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Temporary home for the CAssistant static text-conversion helpers that the
// model layer (CFindItem/CGameObject) needs this round (they live in
// assist/assist.* in the Qt app, ported in the assist-core round). Bodies are
// the originals with a Qt-diet; the two UI-list lookups
// (g_TabListsObjects->SeekObject / g_TabListsTypes->SeekType) return 0 until
// the lists round ports them - numeric parsing still applies.

#ifndef XUOASSIST_TEXTCONV_H
#define XUOASSIST_TEXTCONV_H

#include <stdint.h>
#include <string>

namespace xuoassist {

uint TextToUInt(const std::string &text);
uint TextToSerial(std::string text);
ushort TextToGraphic(std::string text);
std::string SerialToText(const uint &value);
std::string GraphicToText(const ushort &value);

// Forward-ported FIND_TYPE_FLAGS constants (assist/assist.h in the Qt app);
// needed by CGameObject::FindType now, full enum moves with the assist round.
enum FIND_TYPE_FLAGS
{
    FTF_NORMAL = 0x0000,
    FTF_NEAREST = 0x0001,
    FTF_FAST = 0x0002,
    FTF_MOBILE = 0x0004,
    FTF_ITEM = 0x0008,
    FTF_HUMAN = 0x0010,
    FTF_LIVE = 0x0020,
    FTF_DEAD = 0x0040,
    FTF_INJURED = 0x0080,
    FTF_NEXT = 0x0100
};

} // namespace xuoassist

#endif // XUOASSIST_TEXTCONV_H
