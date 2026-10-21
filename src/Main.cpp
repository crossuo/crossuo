// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "white_label.h"
#include <iostream>
#include <time.h>
#if defined(XUO_EMSC)
#include <SDL_timer.h>
#include <emscripten.h>
#include <emscripten/websocket.h>

// workers have no alert(); route error dialogs to the log instead of dying
// with an uncaught ReferenceError
// (the EM_JS macro machinery emits file-scope variables without prior
// declarations - clang's -Wmissing-variable-declarations flags that)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
#endif
EM_JS(void, web_install_alert_shim, (), {
    if (typeof globalThis.alert !== 'function')
    {
        globalThis.alert = function (msg) { console.warn('alert:', msg); };
    }
});
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

// Web file topology (PROXY_TO_PTHREAD): every C-level file operation from
// the pthread worker is served by the MAIN browser thread's filesystem
// (emscripten syscall proxying), so the authoritative state - the streamed
// install, crossuo.cfg, logs - lives in the page's MEMFS, not in this
// worker. Persistence (idbfs) is owned by the page too: resources/shell.html
// mounts IndexedDB over /data, restores the persisted mutable state at
// startup and re-persists it on a slow tick (plus a best-effort flush on
// pagehide / visibilitychange(hidden)). fs_path_appdata() resolves to /data
// on the web (common/fs.h), so everything the client generates under
// FilePath() and the assistant config root lands in /data/crossuo and
// persists; the root-level crossuo.cfg / crossuo.log / xuoassist.ini and the
// /screenshots tree are whitelisted separately.
// The streamed install is deliberately NEVER persisted: it is re-fetchable
// (the browser http cache keeps the fetched blobs; boot re-fetches on
// demand - see the install relocation in load_config: it lives in the
// session-local '/install' tree, outside the idbfs whitelist) and
// serializing hundreds of MB of blob store on a ui tick would stall the
// page.
#endif
#include <common/str.h>
#include <common/utils.h> // MACRO_TOSTRING (via MACRO_STRINGIFY)
#include "Config.h"
#include "revision.h"
#include "Globals.h" // g_Ticks
#include "Misc.h" // g_MainThread
#include "CrossUO.h"
#include "GameWindow.h"
#include "LauncherWindow.h"
#if defined(XUO_ASSISTANT)
#include "AssistantWindow.h"
#include "AssetViewer.h" // AssetViewer_Shutdown (frees the viewer's cached textures)
#endif
#if defined(XUO_LAUNCHER_EMBEDDED)
#include "xuol_app.h"
#include "shards.h" // xuol_set_shards_cfg
#endif
#include <external/popts.h>
#include <xuocore/client_info.h>
#include <xuocore/uodata.h>
#include <xuocore/vfs.h>
#include <common/logging/logging.h>
#include "Managers/ConfigManager.h"
#include "Managers/ConnectionManager.h"
#include "Managers/PacketManager.h"
#include "Network/Packets.h" // CPacketHelpRequest (XUO_AUTO_HELP)
#include "ScreenshotBuilder.h"
#include "Gumps/GumpChat.h" // chat test harness
#include "Managers/GumpManager.h" // chat test harness gump dispatch

#if defined(main) /* SDL renames main to SDL_main for its win32 CRT shim;
                    the client owns the real main() */
#undef main
#endif

extern bool g_var_DisableRenderGame;
bool g_var_DisableRenderGame = false;

extern po::parser g_cli;
po::parser g_cli;

static bool InitCli(int argc, char *argv[])
{
    g_cli["dump-uop"].type(po::string).description("Dump information about a UOP file");
    g_cli["headless"].description("Headless mode, no window is created");
    g_cli["nocrypt"].description("Disable network cryptography");
    g_cli["help"].abbreviation('h').description("print this help screen").callback([&] {
        std::cout << g_cli << '\n';
    });

    // launcher/account options (the embedded launcher passes these)
    g_cli["host"].abbreviation('s').type(po::string).description("LoginServer address");
    g_cli["port"]
        .abbreviation('p')
        .type(po::u32)
        .description("LoginServer port, defaults to " MACRO_TOSTRING(XUO_DEFAULT_SERVER_PORT))
        .fallback(XUO_DEFAULT_SERVER_PORT);
    g_cli["login"].abbreviation('l').type(po::string).description("Account username/login");
    g_cli["password"]
        .abbreviation('w')
        .type(po::string)
        .description("Account password")
        .fallback("");
    g_cli["savepassword"].type(po::u32).description(
        "Force saving the password in crossuo.cfg (0 to remove, 1 to save)");

    g_cli["autologin"].type(po::u32).description("1 to log in automatically");
    g_cli["character"].type(po::string).description("When using auto login, use this character");
    g_cli["fastlogin"].type(po::u32).description("1 to do a fast login");

    g_cli["shard"].type(po::string).description(
        "shard name: names the per-shard streaming install directory"
        " (overrides the config file; defaults to the built-in shard name"
        " " MACRO_TOSTRING(XUO_DEFAULT_SHARD_NAME) ")");
    g_cli["shards-cfg"].type(po::string).description(
        "Use a local shards.cfg file for the launcher shard list (no network"
        " fetch: implies no GitHub fetch, a fully offline shard list)");
    g_cli["clientversion"].type(po::string).description(
        "client version to emulate (e.g. 1.25.32, 7.0.117.0) - overrides the"
        " config file; drives cryptography, packet layout, client type and"
        " verdata usage");
    g_cli["protocolversion"].type(po::string).description(
        "network protocol version when it differs from the client version");
    g_cli["clienttype"].type(po::string).description(
        "client data type: t2a,re,td,lbr,aos,se,sa,ml,ej (auto from the"
        " client version by default)");
    g_cli["crypt"].type(po::u32).description(
        "force network cryptography (1 = the version-derived cipher,"
        " 0 = plain); mirrors the launcher profile's Crypt option");
    g_cli["language"].type(po::string).description(
        "client locale (.loc) to use, e.g. enu - overrides the config file"
        " (the web shell passes the shard profile's Language)");
    g_cli["reportlanguage"].type(po::string).description(
        "language code reported to the server - overrides the config file"
        " (the web shell passes the shard profile's ReportLanguage)");
    g_cli["seedoverride"].type(po::string).description(
        "replaces the IP-derived login seed, 4 bytes like"
        " '0x00 0x01 0x48 0xaa' (the web shell passes the shard profile's"
        " SeedOverride)");
    g_cli["systemcheckoverride"].type(po::string).description(
        "payload of the client->server 0x03 talk acknowledge, bytes like"
        " '0x03 0xb1' (the web shell passes the shard profile's"
        " SystemCheckOverride)");
    g_cli["packet-dump"]
        .description(
            "hex-dump every network packet (client -> server and"
            " server -> client) to the log, with the account/password block"
            " censored (SAFE_DEBUG_DUMP); also raises the log verbosity the"
            " dump needs");

    g_cli["indirect-rendering"].description("uses indirect rendering pipeline");
    g_cli["quiet"]
        .abbreviation('q')
        .description("Disable logging (almost) completely")
        .callback([&] { g_LogEnabled = eLogSystem::LogSystemNone; });

    // vars
    g_cli["DisableRenderGame"].description("disable game canvas rendering");

    // streamed install; typed so both --streaming and
    // --streaming=<dir> parse (empty value = the default streaming dir)
    g_cli["streaming"].type(po::string).description(
        "streamed install mode, optional value = install dir");
    g_cli["streaming-offline"].description(
        "streaming: never hit the network, cache only");
    g_cli["streaming-no-update"].description(
        "streaming: skip the update check at boot - the cached product"
        " manifest and the persisted plan are kept as-is (missing assets"
        " still stream from the network; this is not --streaming-offline)");
    g_cli["streaming-no-sweep"].description(
        "streaming: disable the continuous background download of the"
        " on-demand tiers (T2/T3) - assets fetch only when the game uses"
        " them (on demand) or an update wave touched them");
    g_cli["streaming-product"].type(po::string).description(
        "streaming: product manifest url or file (the official patch"
        " server by default) - an aops patch server root like"
        " https://uniteuo.com/fc works too");
    g_cli["streaming-trap"].type(po::string).description(
        "streaming: set to 'off' to disable hole traps - reads of undownloaded"
        " data return zeros (escape hatch)");
    g_cli["streaming-workers"].type(po::u32).description(
        "streaming: download workers (default cpu-1, max 8)");
    g_cli["streaming-record"].type(po::string).description(
        "streaming: record asset accesses to <file.csv>");
    g_cli["streaming-record-scenario"].type(po::string).description(
        "streaming recorder scenario name");
    g_cli["streaming-backend"].type(po::string).description(
        "streaming: 'arena' for the memory granule store (web groundwork)");
    g_cli["streaming-subset"].type(po::string).description(
        "streaming: 'web' plans the web product subset (drops the large"
        " legacy files)");

    g_cli["config"]
        .abbreviation('c')
        .type(po::string)
        .description("Use a different configuration file");
    g_cli(argc, argv);

    g_var_DisableRenderGame = g_cli["DisableRenderGame"].was_set();
#if defined(XUO_LAUNCHER_EMBEDDED)
    if (g_cli["shards-cfg"].was_set())
        xuol::xuol_set_shards_cfg(g_cli["shards-cfg"].get().string);
#endif

    return g_cli["help"].size() == 0;
}

#if defined(USE_GL3) || defined(USE_GLES) || defined(USE_GL1)
RenderCmdList *g_renderCmdList = nullptr;
static void *s_renderCmdListData = nullptr;

void gfx_render_list_init()
{
    if (s_renderCmdListData)
    {
        return;
    }

    static const uint32_t s_renderCmdListSize = 512 * 1024;
    s_renderCmdListData = malloc(s_renderCmdListSize);
    assert(s_renderCmdListData);

#if defined(USE_GL1)
    const bool immediateMode = true;
#else
    const bool immediateMode = !g_cli["indirect-rendering"].was_set();
#endif // #if defined(USE_GL1)
    static RenderCmdList s_renderCmdList(
        s_renderCmdListData, s_renderCmdListSize, Render_DefaultState(), immediateMode);
    g_renderCmdList = &s_renderCmdList;
}

void gfx_render_list_destroy()
{
    if (s_renderCmdListData)
    {
        free(s_renderCmdListData);
        s_renderCmdListData = nullptr;
    }
    g_renderCmdList = nullptr;
}
#endif // #if defined(USE_GL3) || defined(USE_GLES) || defined(USE_GL1)

void fatal_error_dialog(const char *message)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message, nullptr);
}

bool load_config()
{
    // builds the streaming params from the command line flags
    auto streaming_params = []() {
        vfs_params params;
        params.enabled = true;
#if defined(XUO_EMSC)
        // web defaults: on-disk (MEMFS) blob store + the web product subset
        // (the full legacy install cannot fit the wasm heap). Traps are off:
        // wasm has no fault-driven paging, so reads go through the explicit
        // vfs_ensure_range pre-resolve path.
        //
        // NOT the arena backend: it keeps every decompressed payload
        // resident in the wasm heap, which cannot hold the boot plan next to
        // the install - the ensure failures that followed left the late packs
        // (fonts, string_dictionary) fully stubbed. The sparse store writes
        // the same bytes to files and keeps the heap flat.
        params.store_backend = vfs_store_backend_sparse;
        params.subset = vfs_subset_web;
        params.traps = false;
#endif
        params.offline = g_cli["streaming-offline"].was_set();
        params.update_check = !g_cli["streaming-no-update"].was_set();
        params.background_sweep = !g_cli["streaming-no-sweep"].was_set();
        if (g_cli["streaming-product"].was_set())
        {
            // the product url as given: a full manifest file url, or a bare
            // root (the web flow's /stream/<root>/, /data/<mount>/) that
            // vfs normalizes onto <root>patchlist at the point of use
            params.product_url = g_cli["streaming-product"].get().string;
        }
        else if (g_Config.DataProductUrlSet && !g_Config.DataProductUrl.empty())
        {
            // shard profile product url (crossuo.cfg ProductUrl=): unlike the
            // vfs fallback below it may be a manifest ROOT (trailing /), so
            // the manifest file name is appended to give the fetch a concrete
            // url (a full manifest url passes through unchanged; an empty
            // value is not a usable url and falls through to the defaults)
            std::string product = g_Config.DataProductUrl;
#if defined(XUO_EMSC)
            // relative product urls are resolved against the page origin
            // (fetch workers are created from blobs and have no base url) -
            // same rule as the web default below
            if (product.find("http") != 0)
            {
                product = std::string(emscripten_run_script_string("location.origin")) + "/" + product;
            }
#endif
            if (!product.empty() && product.back() == '/')
            {
                product += "uo-legacyrelease.prod";
            }
            params.product_url = product;
        }
#if defined(XUO_EMSC)
        else
        {
            // web builds stream from their own origin: there is no launcher
            // and no local install, and argv passthrough is best-effort.
            // relative product urls are resolved against the page origin
            // (fetch workers are created from blobs and have no base url)
            std::string product = XUO_WEB_PRODUCT_URL;
            if (product.find("http") != 0)
            {
                product = std::string(emscripten_run_script_string("location.origin")) + "/" + product;
            }
            params.product_url = product;
        }
#endif
        if (g_cli["streaming-trap"].was_set())
        {
            // --streaming-trap=off: holes stay readable (zeros) - the escape
            // hatch when the fault handler misbehaves
            params.traps = g_cli["streaming-trap"].get().string != "off";
        }
        if (g_cli["streaming-workers"].was_set())
            params.workers = g_cli["streaming-workers"].get().u32;
        // web groundwork: in-memory granule backend + web product subset
        // (excludes the large legacy files from the plan)
        if (g_cli["streaming-backend"].was_set())
            params.store_backend = g_cli["streaming-backend"].get().string == "arena"
                                       ? vfs_store_backend_arena
                                       : vfs_store_backend_sparse;
        if (g_cli["streaming-subset"].was_set())
            params.subset =
                g_cli["streaming-subset"].get().string == "web" ? vfs_subset_web : vfs_subset_none;
        // popts: .get() on an option that was never set reads out of bounds
        // (release builds compile the guard out) - always check was_set first
        astr_t dir;
#if defined(XUO_EMSC)
        // an explicit --streaming dir keeps its meaning; only the
        // FilePath()-derived dir is relocated (see the comment below)
        const bool install_dir_from_cli =
            g_cli["streaming"].was_set() && !g_cli["streaming"].get().string.empty();
#endif
        if (g_cli["streaming"].was_set())
            dir = g_cli["streaming"].get().string;
        if (dir.empty())
        {
            // the streaming install is per-shard: the shard name (launcher
            // profile, config file, command line or the white-label default)
            // names the directory, so installs do not mix between shards
            astr_t shard = SanitizeShardName(g_Config.ShardName);
            dir = fs_path_ascii(FilePath(shard.c_str()));
        }
#if defined(XUO_EMSC)
        // keep the streamed install OUT of the persisted state: the web
        // persist path (shell.html) syncs only the small mutable state under
        // the persisted appdata root (/data), and the install is re-fetchable
        // session-local data - the browser http cache persists the fetched
        // blobs, boot re-fetches what it needs on demand. See the web file
        // topology note at the top of this file. The relocation is
        // unconditional for the derived path: fs_path_appdata() resolves
        // inside the persisted idbfs tree since the statefulness work, so a
        // leading slash no longer marks a session-local dir. An explicit
        // --streaming dir keeps its absolute meaning.
        if (!install_dir_from_cli && !dir.empty() && dir.rfind("/install", 0) != 0)
        {
            if (dir[0] == '/')
                dir.erase(0, 1);
            dir = "/install/" + dir;
        }
#endif
        params.install_dir = fs_path_from(dir.c_str());
        // cache (blob store + state db) lives under the streaming dir
        params.blobs_dir = fs_path_join(params.install_dir, "cache", "blobs");
        if (g_cli["streaming-record"].was_set())
        {
            params.record_path =
                fs_path_from(g_cli["streaming-record"].get().string.c_str());
            if (g_cli["streaming-record-scenario"].was_set())
                params.record_scenario =
                    g_cli["streaming-record-scenario"].get().string;
        }
        return params;
        };

        const bool config_exists = LoadGlobalConfig();

    // command line wins over the config file for the shard name
    if (g_cli["shard"].was_set() && !g_cli["shard"].get().string.empty())
    {
        g_Config.ShardName = g_cli["shard"].get().string;
    }

    // ... and for the version markers (the web shell passes the shard
    // profile's clientversion/protocolversion/clienttype here; there is no
    // launcher to write a crossuo.cfg on that path). The profile's crypt
    // option rides along, and so does autologin: UseCrypt gates the version
    // ciphers (with it false the fixups pin the encryption to NOCRYPT
    // regardless of the version) and AutoLogin drives the auto-CONNECT,
    // whose decision point (MainScreen::Init via g_GameWindow.Create below)
    // runs BEFORE ProcessCommandLine - both must be applied here, from the
    // config-load stage
    if (g_cli["crypt"].was_set())
    {
        g_Config.UseCrypt = g_cli["crypt"].get().u32 != 0;
    }
    // locale/protocol overrides: same shape as the config-file keys
    // (ApplyLanguageSettings reads LocaleOverride/LocaleOverrideReport after
    // this point; the byte lists are consumed verbatim at the connection /
    // 0x03 acknowledge sites) - the web shell has no launcher to write a
    // crossuo.cfg, so the shard profile rides in on the command line
    if (g_cli["language"].was_set() && !g_cli["language"].get().string.empty())
    {
        g_Config.LocaleOverride = str_upper(g_cli["language"].get().string);
    }
    if (g_cli["reportlanguage"].was_set() && !g_cli["reportlanguage"].get().string.empty())
    {
        g_Config.LocaleOverrideReport = str_upper(g_cli["reportlanguage"].get().string);
    }
    if (g_cli["seedoverride"].was_set())
    {
        g_Config.SeedOverride = str_trim(g_cli["seedoverride"].get().string);
    }
    if (g_cli["systemcheckoverride"].was_set())
    {
        g_Config.SystemCheckOverride = str_trim(g_cli["systemcheckoverride"].get().string);
    }
    if (g_cli["autologin"].was_set())
    {
        g_Config.AutoLogin = g_cli["autologin"].get().u32 != 0;
    }
    // account credentials: stashed for the screen-init path (see
    // Config::CliLogin - the web boot builds the main screen before
    // ProcessCommandLine reaches SetAccounting) and applied there
    if (g_cli["login"].was_set())
    {
        g_Config.CliLogin = g_cli["login"].get().string;
        g_Config.CliPassword = g_cli["password"].get().string;
        g_Config.CliLoginSet = true;
    }
    ApplyClientVersionOverride(
        g_cli["clientversion"].was_set() ? g_cli["clientversion"].get().string : astr_t(),
        g_cli["protocolversion"].was_set() ? g_cli["protocolversion"].get().string : astr_t(),
        g_cli["clienttype"].was_set() ? g_cli["clienttype"].get().string : astr_t());
    LogClientVersionInfo();

    // a config whose data path carries a streamed (vfs-managed) install -
    // its cache/state marks it - keeps streaming into that directory: the
    // install may be partial (holes stream on demand), so plain mapping
    // would leave it broken
    const bool custom_path_is_streamed =
        config_exists && g_Config.CustomPathSet &&
        fs_path_exists(fs_path_join(g_UOPath, "cache", "state", "plan.bin"));

    // streaming-mode selection, in order of precedence:
    //  1] --streaming on the command line: stream into the given (or
    //     default) directory, even when a data path is configured
    //  2] a config file whose data path is a streamed installation: stream
    //     into the configured directory
    //  3] a config file with any other data path: use that installation,
    //     no streaming (the original behavior)
    //  4] otherwise (no config file, or a config without a data path):
    //     stream automatically - the streamed install replaces the
    //     launcher-driven setup
    const bool streaming = g_cli["streaming"].was_set() || custom_path_is_streamed ||
                           !(config_exists && g_Config.CustomPathSet);
    if (streaming)
    {
        auto params = streaming_params();
        if (!g_cli["streaming"].was_set() && custom_path_is_streamed)
        {
            // keep the streamed install where the config points
            params.install_dir = g_UOPath;
            params.blobs_dir = fs_path_join(params.install_dir, "cache", "blobs");
        }
        vfs_set_params(params);
        g_UOPath = fs_path_from(vfs_install_path());
        // no (explicit) client version anywhere: assume the good defaults
        // the streamed install runs with (latest client version, and
        // everything derived from it - cryptography, client type, packet
        // layout, view range). Only the modified marker decides: on the web
        // there is no config file at all, and the CLI override above must
        // survive this point
        if (!g_Config.ClientVersionModified)
        {
            ApplyClientVersionDefaults();
        }
        uo_data_init(fs_path_ascii(g_UOPath), g_Config.ClientVersion, g_Config.UseVerdata);
        Info(Client, "\tStreaming Data Path: %s", fs_path_ascii(g_UOPath));
        // the streamed product is the modern uop distribution; a config
        // pinned to a pre-uop client version cannot consume it. An EXPLICIT
        // product (--streaming-product, the shard deployment's own data) is
        // the shard owner's vouch: their version stands, however old
        if (!g_cli["streaming-product"].was_set() &&
            g_Config.ClientVersion < VERSION(7, 0, 0, 0))
        {
            g_Config.ClientVersion = CV_LATEST;
            char str[32] = {};
            client_version_string(g_Config.ClientVersion, str, sizeof(str));
            g_Config.ClientVersionString = str;
            Warning(Client, "streaming requires a 7.x client version, using %s instead", str);
        }
        return true;
    }

#if !defined(XUO_LAUNCHER_EMBEDDED)
    if (!config_exists)
    {
        // builds without the embedded launcher have nothing to create a
        // configuration: point at the config file (crossuo.cfg in the current
        // directory, or --config) and bail out
        ErrorMessage(
            Client,
            "CrossUO requires a configuration file to run.\n"
            "Provide a crossuo.cfg (or pass one with --config); there is no"
            " launcher to configure the client.",
            "Could not find configuration file (crossuo.cfg).");
        return false;
    }
#else
    if (!config_exists)
    {
        Info(Client, "no configuration found, opening the embedded launcher");
        LauncherWindow_Init();
        return false;
    }
#endif

    if (config_exists)
    {
        const auto *const uopath = fs_path_ascii(g_UOPath);
        bool valid_client = uopath && uopath[0] && fs_path_exists(g_UOPath);
        if (!valid_client)
        {
            Warning(Client, "Couldn't find data files in the path: %s", uopath);
#if !defined(XUO_LAUNCHER_EMBEDDED)
            ErrorMessage(
                Client,
                "Couldn't find Ultima Online(tm) data files.\n"
                "Please download the original ultima client and configure CrossUO to use the "
                "original's client installation directory.",
                "could not find data path: %s.",
                uopath);
#else
            LauncherWindow_Init();
#endif
            return false;
        }

        const auto client_exe = fs_path_join(uopath, "client.exe");
        const bool client_exe_exists = fs_path_exists(client_exe);
        const bool check_valid_data = !g_Config.ClientVersionModified;
        if (check_valid_data)
        {
            Info(
                Client,
                "client version was not set in config, trying to auto-detect from '%s'",
                fs_path_ascii(client_exe));
            if (client_exe_exists)
            {
                client_info info;
                client_version(fs_path_ascii(client_exe), info);
                if (info.version)
                {
                    g_Config.ClientVersion = info.version;
                    Info(
                        Client,
                        "Client version wasn't set, using auto-detected version from data.");

                    char str[32] = {};
                    client_version_string(info.version, str, sizeof(str));
                    Info(Client, "Using client version: %s.", str);
                    g_Config.ClientVersionString = str;
                }
                else
                {
                    Warning(
                        Client,
                        "Client version not found, signature: 0x%016llx, 0x%08x",
                        (unsigned long long)info.xxh3,
                        info.crc32);
                }
            }
            else
            {
                Info(
                    Client,
                    "a valid client.exe was not found, please set 'ClientVersion' in the config file or in launcher.");
            }
        }

        Info(Client, "validating data compatibility:");
        const bool new_client = fs_path_exists(fs_path_join(uopath, "MainMisc.uop")); // >=7
        Info(Client, "  >=7.x MainMisc.uop:%s found", new_client ? "" : " not");
        const bool old_client3 = fs_path_exists(fs_path_join(uopath, "chardata.mul")); // <=3
        Info(Client, "  <=3.x chardata.mul:%s found", old_client3 ? "" : " not");
        const bool old_client6 = fs_path_exists(fs_path_join(uopath, "map4.mul")) &&
                                 !fs_path_exists(fs_path_join(uopath, "map5.mul")); // <=6
        Info(Client, "  >3.x && <= 6.x map4.mul:%s found", old_client6 ? "" : " not");
        const bool old_client = old_client3 || old_client6;
        valid_client &= g_Config.ClientVersion >= VERSION(7, 0, 0, 0) ? new_client : old_client;
        if (!valid_client)
        {
            if (check_valid_data)
            {
#if !defined(XUO_LAUNCHER_EMBEDDED)
                ErrorMessage(
                    Client,
                    "Couldn't find valid Ultima Online(tm) data files matching your current configuration.\n"
                    "Make sure your configuration is pointing to the correct data files for the"
                    "correct version of your client installation directory.",
                    "could not find data files for client version %s.",
                    g_Config.ClientVersionString.c_str());
#else
                LauncherWindow_Init();
#endif
                return false;
            }
            else
            {
                Warning(
                    Client,
                    "couldn't validate that your data files are compatible with client version");
                Warning(
                    Client, "client will continue but errors may happen - please report any issue");
            }
        }
    }

    // non-streaming path: a plain (mapped) install. The data layer was never
    // initialized above (uo_data_init lives in the streaming branch and used
    // to be folded into LoadGlobalConfig); without it s_ClientVersion stays 0
    // and UOFilePath() reads a stale/empty path, so CFileManager::Load() runs
    // the mul fallback against the wrong directory and fails. Do it here, once
    // g_UOPath and the (possibly auto-detected) client version are final.
    const auto *const uopath = fs_path_ascii(g_UOPath);
    if (uopath && uopath[0] && fs_path_exists(g_UOPath))
    {
        uo_data_init(uopath, g_Config.ClientVersion, g_Config.UseVerdata);
    }

    return true;
}

// Optional frame statistics, enabled with the XUO_FPS_LOG environment variable.
// Prints aggregated frame timings and renderer counters every 5 seconds to stdout.
static void FrameStatsTick()
{
#if defined(USE_GL3) || defined(USE_GLES)
    static const bool enabled = getenv("XUO_FPS_LOG") != nullptr;
    if (!enabled)
    {
        return;
    }

    static uint32_t lastFrameTick = 0;
    static uint32_t windowTick = 0;
    static uint32_t frames = 0;
    static uint32_t minMs = 0xFFFFFFFF;
    static uint32_t maxMs = 0;
    static uint64_t sumMs = 0;

    const uint32_t now = SDL_GetTicks();
    if (lastFrameTick != 0)
    {
        const uint32_t ms = now - lastFrameTick;
        frames++;
        sumMs += ms;
        if (ms < minMs)
        {
            minMs = ms;
        }
        if (ms > maxMs)
        {
            maxMs = ms;
        }
    }
    lastFrameTick = now;

    if (windowTick == 0)
    {
        windowTick = now;
        return;
    }

    if (now - windowTick >= 5000)
    {
        const uint32_t elapsed = now - windowTick;
        RenderFrameStats stats;
        Render_GetFrameStats(&stats);
        printf(
            "[fps] %ums: frames=%u fps=%.1f frame avg=%.2fms min=%ums max=%ums | "
            "draws=%llu uploads=%llu bytes=%llukB batches=%llu batchedQuads=%llu\n",
            elapsed,
            frames,
            frames * 1000.0 / elapsed,
            frames ? double(sumMs) / frames : 0.0,
            minMs == 0xFFFFFFFF ? 0 : minMs,
            maxMs,
            (unsigned long long)stats.drawCalls,
            (unsigned long long)stats.vertexUploads,
            (unsigned long long)(stats.bytesUploaded / 1024),
            (unsigned long long)stats.batchFlushes,
            (unsigned long long)stats.batchedQuads);
        fflush(stdout);
        Render_ResetFrameStats();
        frames = 0;
        sumMs = 0;
        minMs = 0xFFFFFFFF;
        maxMs = 0;
        windowTick = now;
    }
#endif
}

// test harness (XUO_AUTO_HELP): fires the client's help request once, 15s
// after the world was entered, then captures the framebuffer ~6s later, so the
// server-sent Help gump (network gump path: server-authored scissors, scrolling
// content) can be validated automatically and compared between render paths.
// Combine with XUO_DUMP_FRAME_DELAY for a command-stream dump and
// XUO_GUMP_LEGACY to A/B against the framebuffer path.
// chat test harness (XUO_CHAT_TEST): drives the conference gump with
// synthetic clicks on a fixed timeline after world entry - select channel,
// join, leave, create a channel, leave again - logging every step, so the
// wire exchange can be correlated with the server-side chat log
static void ChatTestTick()
{
    static const bool enabled = getenv("XUO_CHAT_TEST") != nullptr;
    if (!enabled || g_GameState != GS_GAME)
    {
        return;
    }

    struct Step
    {
        uint32_t delay;
        int action; // 0=done, 1=click, 2=open chat, 3=create channel
        int id;
        const char *name;
    };
    static const Step steps[] = {
        { 2000, 2, 0, nullptr },      // open the conference gump
        { 4000, 1, 0x0100, nullptr }, // select first channel row
        { 6000, 1, 1, nullptr },      // join
        { 10000, 1, 2, nullptr },     // leave
        { 13000, 1, 3, nullptr },     // open creation box
        { 15000, 3, 6, "XUOTest" },   // fill + confirm creation box
        { 18000, 1, 2, nullptr },     // leave again
        { 21000, 1, 0x0100, nullptr }, // select first channel row
        { 23000, 1, 1, nullptr },      // join
        { 26000, 4, 0, "XUO chat test message" }, // speak in the conference
        { 0, 0, 0, nullptr },
    };
    static uint32_t base = 0;
    static size_t next = 0;

    if (base == 0)
    {
        base = g_Ticks;
        Info(Client, "XUO_CHAT_TEST: timeline started");
    }

    while (next < (sizeof(steps) / sizeof(steps[0])) - 1 && (g_Ticks - base) >= steps[next].delay)
    {
        const Step &step = steps[next];
        next++;
        switch (step.action)
        {
            case 2:
            {
                Info(Client, "CHATTEST: open chat");
                g_Game.OpenChat();
                break;
            }
            case 1:
            {
                CGump *gump = g_GumpManager.GetGump(0, 0, GT_CHAT);
                if (gump != nullptr)
                {
                    ((CGumpChat *)gump)->DebugClick(step.id);
                }
                else
                {
                    Info(Client, "CHATTEST: conference gump not open for click %d", step.id);
                }
                break;
            }
            case 3:
            {
                CGump *gump = g_GumpManager.GetGump(0, 0, GT_CHAT);
                if (gump != nullptr)
                {
                    ((CGumpChat *)gump)->DebugCreateChannel(step.name);
                }
                break;
            }
            case 4:
            {
                CGump *gump = g_GumpManager.GetGump(0, 0, GT_CHAT);
                if (gump != nullptr)
                {
                    ((CGumpChat *)gump)->DebugSendMessage(step.name);
                }
                break;
            }
            default:
                break;
        }
    }
}

static void AutoHelpTick()
{
    static const bool enabled = getenv("XUO_AUTO_HELP") != nullptr;
    if (!enabled)
    {
        return;
    }

    static bool sent = false;
    static uint32_t requestAt = 0;
    static uint32_t captureAt = 0;

    if (!sent)
    {
        if (g_GameState < GS_GAME)
        {
            return;
        }

        if (requestAt == 0)
        {
            requestAt = g_Ticks + 15000;
            Info(Client, "XUO_AUTO_HELP: requesting the help gump in 15s");
        }
        else if (g_Ticks >= requestAt)
        {
            sent = true;
            Info(Client, "XUO_AUTO_HELP: sending the help request");
            CPacketHelpRequest().Send();
            captureAt = g_Ticks + 6000;
        }
    }
    else if (captureAt != 0 && g_Ticks >= captureAt)
    {
        captureAt = 0;
        Info(Client, "XUO_AUTO_HELP: capturing the help gump");
        g_ScreenshotBuilder.SaveScreen();
    }
}

#if defined(XUO_EMSC)
#include <emscripten.h>
#endif

// one frame of the client loop; returns true when the client wants to quit
static bool RunFrame(bool &raised)
{
    bool quit = false;
    FrameStatsTick();

    // 1] Handle all SDL events. Mouse, keyboard, windows etc..
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
#if defined(XUO_LAUNCHER_EMBEDDED)
        // events for the launcher window (and the launcher hotkey) never
        // reach the game window procedure
        if (LauncherWindow_ProcessEvent(event))
            continue;
#endif
#if defined(XUO_ASSISTANT)
        // events for assistant pop-out windows (and assistant key
        // bindings) are consumed before the game window procedure sees
        // them; while the
        // assistant is up it also keeps the events ImGui captured
        if (AssistantWindow_ProcessEvent(event))
            continue;
#endif
        quit = g_GameWindow.OnWindowProc(event);
        if (quit)
        {
            break;
        }
    }

    // 2] Run main loop, packets, rendering etc..
    g_Ticks = SDL_GetTicks();
    g_ConnectionManager.Recv();
    g_PacketManager.SendMegaClilocRequests();
    g_GameWindow.PollWebKeyboard();
    g_Game.Process(true);
    AutoHelpTick();
    ChatTestTick();

#if defined(XUO_LAUNCHER_EMBEDDED)
    LauncherWindow_Render();
#endif

    // 3] Calculate proper delay based on settings and frame time.
    // the test harness (XUO_AUTO_LOGIN) runs uncapped: the window is not
    // focused during automated runs, which would apply the inactive-window
    // delay and mask the real frame cost being benchmarked
    if (!TestAutoLogin())
    {
        int32_t iDelay = g_GameWindow.GetRenderDelay();

        // a frame delay of 0 means uncapped; otherwise only sleep to hit
        // the configured cap (the old 2ms floor forced ~140fps ceilings)
        if (iDelay > 0)
        {
            const int32_t delay = iDelay - (SDL_GetTicks() - g_Ticks);
            if (delay > 0)
            {
                SDL_Delay(delay);
            }
        }
    }

    if (!raised)
    {
        raised = true;
        g_GameWindow.Raise(); // once, now that the first frame is on screen
    }
    return quit;
}

static int Run()
{
    // one-time bring-to-front after the first frame is shown: the raise in
    // CWindow::Create runs before the window is mapped and some window
    // managers ignore it, leaving the client behind other applications
    bool raised = false;
#if defined(XUO_EMSC)
    // the browser owns the thread: hand the loop over (0 fps =
    // requestAnimationFrame pacing). simulate_infinite_loop keeps the stack
    // alive across frames; the shutdown path below runs when the loop is
    // cancelled (via an UNWIND on quit, teardown is handled by the runtime)
    emscripten_set_main_loop_arg(
        [](void *arg)
        {
            if (RunFrame(*static_cast<bool *>(arg)))
            {
                emscripten_cancel_main_loop();
            }
        },
        &raised,
        0,
        1);
    // rAF pacing in a worker is quantized: a frame that misses its 1/60s
    // slot is pushed to the NEXT slot, so any frame cost over ~18.7ms
    // locks the cadence at 33.3ms (~30fps). Switch to setTimeout(0)
    // pacing: the loop then runs as fast as frames complete (browser
    // clamps nested timers to ~4ms), still implicitly capped by WebGL
    // present throttling.
    emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, 0);
#else
    while (!RunFrame(raised))
    {
    }
#endif
#if defined(XUO_LAUNCHER_EMBEDDED)
    LauncherWindow_Shutdown();
#endif
#if defined(XUO_ASSISTANT)
    AssistantWindow_Shutdown();
    // after the assistant shutdown: the ImGui context is gone, so this only
    // releases the viewer's CPU-side texture pixels
    AssetViewer_Shutdown();
#endif
    return EXIT_SUCCESS;
}

#if defined(XUO_EMSC)
#endif

int main(int argc, char **argv)
{
#if defined(XUO_EMSC)
    web_install_alert_shim();
    #endif
#if 0 // TEST encoding
    str_init();
    // Add some tests, this cliloc message seems to use broken UTF8
    // "Hey buddy... Looking for work?"
    uint8_t d[] = {
        0x48, 0x65, 0x79, 0x20, 0x62, 0x75, 0x64, 0x64, 0x79, 0x2E, 0xC2,
        0xA0, 0x20, 0x4C, 0x6F, 0x6F, 0x6B, 0x69, 0x6E, 0x67, 0x20, 0x66,
        0x6F, 0x72, 0x20, 0x77, 0x6F, 0x72, 0x6B, 0x3F
    };
    // cp1250 - issue #221
    //unsigned char a[] = { 0x56, 0xE1, 0x6c, 0x65, 0xE8, 0x6E, 0xED, 0x6B }; // input
    astr_t s1( reinterpret_cast< char const* >( d ), sizeof( d ) );
    str_terminate();
    exit(0);
#endif

#if defined(XUO_EMSC)
    web_install_alert_shim();
    // force library_websocket.js inclusion: Sockets.cpp calls the
    // emscripten_websocket_* functions from a different TU and the
    // JS optimizer only scans the main compilation unit
    emscripten_websocket_is_supported();
#endif
    g_fatalErrorCb = &fatal_error_dialog;
    if (!InitCli(argc, argv))
    {
        return 0;
    }
#if defined(XUO_LAUNCHER_EMBEDDED)
    xuol::client_set_restart_args(argc, argv);
#endif

    // --packet-dump: enable the Network system and raise the stderr cutoff so
    // the compiled-in hex dumps (DEBUG_DUMP/SAFE_DEBUG_DUMP in
    // CPacketManager::OnPacket and CGame::ProcessPacket) actually print.
    // The dump level is 2 (SAFE_*) / 1 (DEBUG_*); both need the system bit,
    // and SAFE needs cutoff >= 2. g_LogEnabled is the runtime gate.
    if (g_cli["packet-dump"].was_set())
    {
        g_LogEnabled = eLogSystem::LogSystemAll;
        LogInit(argc, argv, "crossuo.log", 3);
    }
    else
    {
        LogInit(argc, argv, "crossuo.log");
    }

    if (SDL_Init(SDL_INIT_TIMER) < 0)
    {
        ErrorMessage(
            Client,
            "Window initialization failure.",
            "unable to initialize SDL %s",
            SDL_GetError());
        return EXIT_FAILURE;
    }
    Info(Client, "SDL initialized");

    str_init();
    Info(Client, "initializing application");
    g_MainThread = SDL_ThreadID();
    g_ExePath = fs_path_current();
    g_UOPath = fs_path_current();

    const bool isHeadless = g_cli["headless"].was_set();

    if (!load_config())
    {
#if defined(XUO_LAUNCHER_EMBEDDED)
        // no valid configuration: the validation already ran once, open the
        // launcher and wait; the configuration is only validated again when
        // the user applies a profile (save or launch) from the launcher
        if (isHeadless || !LauncherWindow_Init())
            return EXIT_FAILURE;
        bool valid_config = false;
        while (!valid_config)
        {
            if (!LauncherWindow_Pump())
                break; // user closed the launcher
            if (xuol_app_consume_profile_applied())
                valid_config = load_config();
        }
        if (!valid_config)
        {
            LauncherWindow_Shutdown();
            return EXIT_FAILURE;
        }
#else
        return EXIT_FAILURE;
#endif
    }

    if (g_cli["dump-uop"].was_set())
    {
        g_dumpUopFile = g_cli["dump-uop"].get().string;
    }

    if (!isHeadless)
    {
#if defined(USE_GL3) || defined(USE_GLES) || defined(USE_GL1)
        gfx_render_list_init();
#endif // #if defined(USE_GL3) || defined(USE_GLES) || defined(USE_GL1)
        if (!g_GameWindow.Create(ClientWindowTitle().c_str(), false, 640, 480))
        {
            const char *errMsg =
                "Failed to create CrossUO client window. May be caused by a missing configuration file.";
            ErrorMessage(Client, errMsg, "error initializing game window");
            return -1;
        }
    }
    g_ConfigManager.Init();

    // FIXME: headless: lets end here so we can run on travis for now
    if (isHeadless)
    {
        return EXIT_SUCCESS;
    }

    g_Game.ProcessCommandLine();
    g_GameWindow.Raise();
#if !defined(XUO_EMSC)
    auto ret = Run();
    SDL_Quit();
    str_terminate();
#if defined(USE_GL3) || defined(USE_GLES) || defined(USE_GL1)
    gfx_render_list_destroy();
#endif // #if defined(USE_GL3) || defined(USE_GLES) || defined(USE_GL1)
    return ret;
#else
    Run();
    SDL_Quit();
    str_terminate();
#if defined(USE_GL3) || defined(USE_GLES) || defined(USE_GL1)
    gfx_render_list_destroy();
#endif
    return EXIT_SUCCESS; // the browser keeps the loop alive; main never returns
#endif
}
