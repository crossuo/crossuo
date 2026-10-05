// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Danny Angelo Carminati Grein

// GL texture name tracking: follows every texture name from creation to
// deletion so a failing bind can be reported with the full story — who
// generated the name, who deleted it, and the create/delete history leading up
// to the failure.
//
// The report is only produced when something actually fails (a bad bind, or a
// GL_CHECK error), and the process then aborts, so only the FIRST failure is
// ever seen and the log stays free of per-frame spam.
//
// On the GL3/gl3w build the loader exposes reassignable function pointers, so
// glGenTextures/glDeleteTextures are wrapped and NO call site can be missed.
// On GL1/GLEW those symbols are real GLAPI functions and cannot be intercepted
// from here; GL1 gets the bind-side checks only.
//
// XUO_TRACK_TEXTURES=1                       enable the tracker
// XUO_GL_NO_ABORT=1                          log the story without aborting
// XUO_GL_VALIDATE=0                          disable GL_CHECK error checking

#pragma once

#if defined(USE_GL3) || defined(USE_GLES) || defined(USE_GL1)

// installs the wrappers; call once after the GL loader is initialized
void GLTextureTracker_Install();

// reports one bind of `name` (call right before glBindTexture); silent unless
// the name is dead, in which case the full story is printed
void GLTextureTracker_OnBind(const char *what, unsigned int name);

// counts frames for the history; call once per executed frame
void GLTextureTracker_OnFrame();

// dumps a one-line census (no per-name detail); used for manual probing
void GLTextureTracker_Dump();

#endif
