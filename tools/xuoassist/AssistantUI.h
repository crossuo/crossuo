// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef _ASSISTANT_UI_H_
#define _ASSISTANT_UI_H_

// Internal helper shared by AssistantWindow.cpp (lifecycle) and
// AssistantUI.cpp (window content); not part of the client-facing API.
// No state is shared: the lifecycle owns the visibility flag and passes the
// session start ticks in, so both translation units stay independent.

#include <stdint.h>

#if defined(XUO_ASSISTANT)

// Draws the assistant main window (dockable, pop-out aware). open is the
// visibility flag owned by AssistantWindow.cpp (the window X button hides the
// assistant through it); sessionStartTicks is the g_Ticks value captured when
// the assistant was initialized.
void AssistantUI_Draw(bool *open, uint32_t sessionStartTicks);

#endif // XUO_ASSISTANT
#endif // _ASSISTANT_UI_H_
