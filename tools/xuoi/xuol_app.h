// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <external/gfx/gfx.h>

//
// X:UO Launcher application lifecycle.
//
// The launcher UI is hosted embedded in the crossuo client
// (src/LauncherWindow.cpp) on GL3 desktop builds (XUO_LAUNCHER_EMBEDDED);
// other renderer builds run without a launcher.
//
// The host owns the OS window and the GL context, and drives the app:
//   xuol_app_init(win)          -> create ImGui context, load config/accounts/shards
//   xuol_app_process_event(e)   -> feed each SDL event
//   xuol_app_frame()            -> new frame, draw views, render into the current context
//   xuol_app_wants_close()      -> app close intent (auto-close after launch, quit)
//   xuol_app_shutdown()         -> flush config, destroy ImGui context
//

bool xuol_app_init(win_context &win, const char *ini_name = nullptr, bool enable_viewports = true);
void xuol_app_process_event(const SDL_Event *event);
void xuol_app_frame();
bool xuol_app_wants_close();
void xuol_app_clear_close_intent();
#if defined(XUO_LAUNCHER_EMBEDDED)
// ask for a client reload (used by views whose action needs a restart); the
// app frame performs the actual restart through the client bridge
void xuol_request_reload();
#endif
void xuol_app_shutdown();
void xuol_ui_clear_color(float out[4]);
// set by the client bridge whenever a profile was applied (save or launch);
// the boot flow consumes it to know when to re-validate the configuration
void xuol_app_notify_profile_applied();
bool xuol_app_consume_profile_applied();

#if defined(XUO_LAUNCHER_EMBEDDED)
// Implemented by the client (src/LauncherWindow.cpp) to apply launcher
// profiles to the running client instead of spawning a new process.
#include <xuocore/common.h>

namespace xuol {
struct client_profile
{
    astr_t profile;
    astr_t login;
    astr_t password;
    bool remember_pw = false;
    bool autologin = false;
    astr_t character; // fast login character
    astr_t loginserver;
    astr_t data_path;
    astr_t clientversion;
    astr_t clienttype;
    bool crypt = false;
};

enum class apply_result
{
    applied,       // profile is live in the client
    needs_restart, // profile persisted, but the client must be restarted
    failed,        // could not apply (error logged)
};

apply_result client_apply_profile(const client_profile &profile);
// a complete shard description for the shard views; field names mirror the
// client's Config::shard_profile (xuol keeps its own type so the launcher
// side stays independent of the client headers)
struct shard_profile
{
    astr_t name;
    astr_t loginserver;
    astr_t clientversion;
    astr_t clienttype;
    bool crypt = false;
    astr_t product_url;
    astr_t web;
    astr_t account_page;
    astr_t help;
    astr_t video;
    astr_t credits;
    astr_t other;     // plain community url ("" = none)
    astr_t community; // forum/community page (launcher-facing)
    astr_t seed_override;         // "0xaa 0xbb 0xcc 0xdd" login seed ("" = none)
    astr_t system_check_override; // 0x03 payload bytes ("" = none)
    // assistant constraint: "" = keep the client's own Assistant= value,
    // "no" = forbid launching the assistant ("yes" is never sent: it is the
    // default everywhere)
    astr_t assistant;
    astr_t language;              // client locale (.loc), "" = keep default
    astr_t report_language;       // report locale, "" = keep default
};
// applies a shard selection to the running client instead of spawning a new
// process; see client_apply_profile for the result semantics
apply_result client_apply_shard(const shard_profile &profile);
// relaunches the client process (used for applying updates and profiles that
// cannot be applied live); the current process exits afterwards
void client_request_restart();
// forwards the original command line so a relaunched client keeps options
// like --config; call once from the entry point
void client_set_restart_args(int argc, char **argv);
} // namespace xuol
#endif // XUO_LAUNCHER_EMBEDDED
