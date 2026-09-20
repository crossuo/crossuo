// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Danny Angelo Carminati Grein

// Startup splash screen.
//
// The image (resources/cover.png) is embedded in the executable at build time
// (see CMake/Embedding.cmake) so it can be drawn as soon as the window and its
// GL context exist, covering the window while the files are loaded and the
// first (login) screen is not ready to render yet.

#pragma once

// Decode the embedded splash image and upload it to the GPU.
// Safe to call more than once; repeats are no-ops.
// Requires a current GL context, returns false if the context is not ready or
// the image could not be decoded (the client keeps running either way).
bool Splash_Init();

// Draw the splash image centered and aspect-fit into the current window and
// present it. Nothing is drawn if Splash_Init() did not succeed.
void Splash_Render();

// Same as Splash_Render() but also pumps pending SDL events so the window stays
// responsive to the window manager while the caller is busy loading data.
void Splash_RenderAndPumpEvents();

// Release the splash GPU texture (called once the first real screen is up).
void Splash_Shutdown();

// True once the splash texture is ready to be drawn.
bool Splash_IsReady();
