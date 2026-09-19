// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Danny Angelo Carminati Grein

#pragma once

#if defined(NEW_RENDERER_ENABLED) && (defined(USE_GL3) || defined(USE_GLES))

#include "RenderTypes.h"
#include "RenderCommands.h"

struct RenderState;
struct DrawQuadCmd;
struct DrawUntexturedQuadCmd;
struct RenderCmdList;

// Deferred quad batching for the immediate execution path: consecutive quads
// drawn with the same texture and the same render state are merged into a
// single vertex upload and a single glDrawArrays(GL_TRIANGLES) call.
// The recorded command stream is unaffected, so frame dumps remain valid.

// record + defer a textured quad (DrawQuadCmd semantics)
void RenderBatch_AddQuad(RenderState *state, const DrawQuadCmd &cmd);

// record + defer an untextured quad (DrawUntexturedQuadCmd semantics)
void RenderBatch_AddUntexturedQuad(RenderState *state, const DrawUntexturedQuadCmd &cmd);

// called before a recorded command is applied; flushes the pending batch when
// the command would change state the batch depends on (no-ops keep it alive)
void RenderBatch_OnCommand(RenderState *state, RenderCommandType type, const void *cmd);

// submits the pending merged quad batch (no-op when empty)
void RenderBatch_Flush(RenderState *state);

#endif // NEW_RENDERER_ENABLED && (USE_GL3 || USE_GLES)
