// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Danny Angelo Carminati Grein

#pragma once

// Builds the renderer's GLSL sources for the dialect of the context that was
// actually created. The shader bodies are dialect-neutral; only the version
// line, the attribute/varying qualifiers and a small preamble differ, so the
// sources are assembled at run time instead of being frozen by the build.
//
// The returned pointers live for the process lifetime.

#ifdef __cplusplus
extern "C" {
#endif

// Builds (once) the vertex and fragment sources for the current glctx flavour.
void shader_src_init(void);

const char *shader_src_vertex(void);
const char *shader_src_fragment(void);

#ifdef __cplusplus
}
#endif
