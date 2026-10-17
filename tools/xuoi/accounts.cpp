// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Launcher

#include "accounts.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <inttypes.h>
#include <external/inih.h>
#include <external/process.h>
#include <external/tinyfiledialogs.h>
#include <xuocore/client_info.h>
#include <xuocore/http.h>
#include <xuocore/vfs_source_aops.h>
#include <xuocore/common.h>
#include "widgets.h"
#include "ui_model.h"
#include "shards.h"
#include "ui_shards.h"
#include "xuol_app.h"
#include "../../white_label.h"

namespace xuol {

#define CFG_NAME account
#define CFG_SECTION_FILTER_NAME "account"
#define CFG_DEFINITION "cfg_launcher.h"
#include "cfg_loader.h"

#define CFG_NAME crossuo
#define CFG_DEFINITION "cfg_crossuo.h"
#include "cfg_loader.h"

#if defined(XUO_WINDOWS)
#define XUO_EXE "crossuo.exe"
#else
#define XUO_EXE "crossuo"
#endif

// xuol_app.cpp
extern void save_config();
extern const fs_path &xuol_data_path();
extern void xuol_launch_quit();
#if defined(XUO_LAUNCHER_EMBEDDED)
extern void xuol_request_reload();
#endif
extern void xuol_set_last_used(int account_index);
extern int xuol_last_used();

static inline bool ui_modal(const char *title, const char *msg, bool &response)
{
    bool closed = false;
    ImGui::OpenPopup(title);
    if (ImGui::BeginPopupModal(title))
    {
        ImGui::Text("%s", msg);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Button("Yes", ImVec2(80, 0)))
        {
            response = true;
            closed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(80, 0)))
        {
            response = false;
            closed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return closed;
}

// model

const char *client_types[] = {
    "Automatic",
    "The Second Age (T2A)",
    "Renaissance (RE)",
    "Third Dawn (TD)",
    "Lord Blackthorn's Revenge (LBR)",
    "Age Of Shadows (AOS)",
    "Samurai Empire (SE)",
    "Stygian Abyss (SA)",
    "Mondain's Legacy (ML)",
    "Endless Journey (EJ)",
};

const char *client_types_cfg[] = {
    "", "t2a", "re", "td", "lbr", "aos", "se", "sa", //"ml", "ej",
};

static account::data s_accounts;
static std::unordered_map<astr_t, int> s_account_by_name;

void load_accounts()
{
    const auto fname = fs_path_join(xuol_data_path(), "xuolauncher.cfg");
    LOG_INFO("loading accounts from %s", fs_path_ascii(fname));
    auto fp = fs_open(fname, FS_READ);
    account::cfg(fp, s_accounts);
    if (fp)
        fs_close(fp);

    if (s_accounts.entries.empty())
    {
        auto default_shard = account::default_entry();
        default_shard.account_clientversion = XUO_DEFAULT_CLIENT_VERSION_STR;
        default_shard.account_clienttype = XUO_DEFAULT_CLIENT_TYPE;
        default_shard.account_loginserver = XUO_DEFAULT_SERVER_ADDRESS "," MACRO_TOSTRING(XUO_DEFAULT_SERVER_PORT);
        default_shard.account_profile = XUO_DEFAULT_SHARD_NAME;
        s_accounts.entries.emplace_back(default_shard);
    }

    auto it = s_accounts.entries.emplace(s_accounts.entries.begin());
    (*it).account_profile = "<new>";
    int i = 0;
    for (auto &e : s_accounts.entries)
    {
        s_account_by_name[e.account_profile] = i++;
        account::dump(&e);
    }
}

void write_accounts(void *_fp)
{
    if (s_accounts.entries.size() < 2) // <new> account is first
        return;

    auto fp = (FILE *)_fp;
    for (size_t i = 1; i < s_accounts.entries.size(); ++i)
    {
        const auto &e = s_accounts.entries[i];
        account::write(fp, e, "Account");
    }
}

int account_index_by_profilename(const char *name)
{
    auto it = s_account_by_name.find(name);
    if (it != s_account_by_name.end())
        return (*it).second;
    return 0;
}

int account_client_type_index_by_cfg(const char *shortname)
{
    for (int i = 0; i < IM_ARRAYSIZE(client_types_cfg); ++i)
    {
        if (!strcasecmp(client_types_cfg[i], shortname))
            return i;
    }
    return 0;
}

int account_client_type_index_by_name(const char *name)
{
    for (int i = 0; i < IM_ARRAYSIZE(client_types); ++i)
    {
        if (!strcasecmp(client_types[i], name))
            return i;
    }
    return 0;
}

// the launch configuration of a profile lives next to the launcher data,
// named after the profile with path-unfriendly characters sanitized away
static fs_path profile_cfg_path(const astr_t &name)
{
    astr_t fname = name + ".cfg";
    std::replace(fname.begin(), fname.end(), '/', '_');
    std::replace(fname.begin(), fname.end(), '\\', '_');
    std::replace(fname.begin(), fname.end(), ' ', '_');
    return fs_path_join(xuol_data_path(), fname);
}

static fs_path account_create_config(const account::entry &account)
{
    const auto fname = profile_cfg_path(account.account_profile);
    auto fp = fs_open(fname, FS_WRITE);
    if (!fp)
    {
        LOG_ERROR("failed to write launch configuration");
        return {};
    }
    LOG_DEBUG("writing launch configuration in: %s", fs_path_ascii(fname));
    auto entry = crossuo::default_entry();
    entry.crossuo_acctid = account.account_login;
    entry.crossuo_acctpassword = account.account_password;
    entry.crossuo_rememberacctpw = !account.account_password.empty();
    entry.crossuo_autologin = account.account_auto_login;
    entry.crossuo_clienttype = account.account_clienttype;
    entry.crossuo_clientversion = account.account_clientversion;
    entry.crossuo_crypt = account.account_crypt;
    entry.crossuo_custompath = account.account_data_path;
    entry.crossuo_loginserver = account.account_loginserver;
    // the launcher profile names the shard: the client streams its data
    // install into a per-shard directory named after it
    entry.crossuo_shardname = account.account_profile;

    crossuo::write(fp, entry, nullptr);
    fclose(fp);
    return fname;
}

// maps a shard list entry onto the crossuo launch configuration schema; keys
// the shard does not provide are left at their default so cfg_loader's write
// skips them and the client keeps its own defaults (an explicit "key=" line
// would clear them client-side)
static void shard_fill_entry(const shard_data &shard, crossuo::entry &e)
{
    // identity: what the client needs to reach the shard and name its data
    // (shard_data pointers come from the shard list, but guard anyway)
    if (shard.name)
        e.crossuo_shardname = shard.name;
    if (shard.loginserver)
        e.crossuo_loginserver = shard.loginserver;
    if (shard.clientversion)
        e.crossuo_clientversion = shard.clientversion;
    if (shard.protocolversion && shard.protocolversion[0])
        e.crossuo_protocolclientversion = shard.protocolversion;
    if (shard.clienttype) // empty: the client autodetects
        e.crossuo_clienttype = shard.clienttype;
    e.crossuo_crypt = shard.crypt;

    // a shard launch never carries credentials: the login screen asks for
    // them, picking a shard is not an account login
    e.crossuo_acctid = {};
    e.crossuo_acctpassword = {};
    e.crossuo_rememberacctpw = false;
    e.crossuo_autologin = false;

    // data install source: where the client streams/downloads its data from;
    // the streaming product is the full path to the manifest file: write the
    // url as provided (the client fetches exactly this url), empty means a
    // manual install: no product url, the client keeps its configured default
    if (shard.product_url && shard.product_url[0])
        e.crossuo_producturl = shard.product_url;

    // presentation links, only when the shard provides them
    if (shard.web && shard.web[0])
        e.crossuo_shardweb = shard.web;
    if (shard.account_page && shard.account_page[0])
        e.crossuo_shardaccount = shard.account_page;
    if (!shard.community.empty())
        e.crossuo_shardcommunity = shard.community;
    if (!shard.other.empty())
        e.crossuo_shardother = shard.other;
    if (shard.video && shard.video[0])
        e.crossuo_shardvideo = shard.video;
    // help/credits have no shard.ini source: the client keeps its defaults
    // the locale is the single default-client-loc knob: forward it as-is
    if (shard.language && shard.language[0])
        e.crossuo_language = shard.language;
    if (shard.report_language && shard.report_language[0])
        e.crossuo_reportlanguage = shard.report_language;
    if (shard.seed_override && shard.seed_override[0])
        e.crossuo_seedoverride = shard.seed_override;
    if (shard.system_check_override && shard.system_check_override[0])
        e.crossuo_systemcheckoverride = shard.system_check_override;
    // only a shard that explicitly disables the assistant writes the key:
    // a shard without an Assistant entry keeps the client's own crossuo.cfg
    // value (the assistant is a client-local preference, not shard data)
}

#if defined(XUO_LAUNCHER_EMBEDDED)
// embedded in the client: apply the profile live instead of spawning
// another process; the client persists it to crossuo.cfg itself and the
// boot/login flow reacts to the new values
//
// when the profile cannot be applied live (different data path, in game,
// connected) the client is simply relaunched: the profile was already
// persisted by client_apply_profile(), so the restarted process picks it up.
// There is no confirmation dialog; the restart is the answer.
static xuol::apply_result account_apply_live(int account_index)
{
    assert(account_index >= 0 && account_index < (int)s_accounts.entries.size());
    const auto &entry = s_accounts.entries[account_index];

    xuol::client_profile profile;
    profile.profile = entry.account_profile;
    profile.login = entry.account_login;
    profile.password = entry.account_password;
    profile.remember_pw = !entry.account_password.empty();
    profile.autologin = entry.account_auto_login;
    profile.character = entry.account_fast_login;
    profile.loginserver = entry.account_loginserver;
    profile.data_path = entry.account_data_path;
    profile.clientversion = entry.account_clientversion;
    profile.clienttype = entry.account_clienttype;
    profile.crypt = entry.account_crypt;

    const auto result = xuol::client_apply_profile(profile);
    if (result == xuol::apply_result::applied)
    {
        LOG_INFO("profile applied: %s", profile.profile.c_str());
    }
    else if (result == xuol::apply_result::needs_restart)
    {
        LOG_INFO("profile saved, the client reloads to apply it");
        xuol_request_reload();
    }
    return result;
}
#endif // XUO_LAUNCHER_EMBEDDED

#if !defined(XUO_LAUNCHER_EMBEDDED)
static fs_path xuo_client_bin()
{
    const fs_path paths[] = {
        fs_path_join(xuol_data_path(), XUO_EXE),
        fs_path_join(fs_path_current(), XUO_EXE),
        fs_path_join(fs_path_process(), XUO_EXE),
    };
    for (int i = 0; i < countof(paths); ++i)
    {
        if (fs_path_is_file(paths[i]))
            return paths[i];
    }
    return {};
}
#endif // !XUO_LAUNCHER_EMBEDDED

void account_launch_shard(int shard_index)
{
    // entry 0 is the "<custom shard>" placeholder, it has nothing to launch
    if (shard_index <= 0 || shard_index >= shard_count())
        return;

    const auto shard = shard_by_id(shard_index);
    if (!shard.loginserver || !shard.loginserver[0])
    {
        LOG_ERROR("shard '%s' has no login server set, cannot launch", shard.name);
        return;
    }

    // map the shard through the same config-file schema the standalone path
    // writes: fields the shard does not provide stay at their default, which
    // the embedded bridge treats as "not provided"
    auto entry = crossuo::default_entry();
    shard_fill_entry(shard, entry);

#if defined(XUO_LAUNCHER_EMBEDDED)
    xuol::shard_profile profile;
    profile.name = entry.crossuo_shardname;
    profile.loginserver = entry.crossuo_loginserver;
    profile.clientversion = entry.crossuo_clientversion;
    profile.clienttype = entry.crossuo_clienttype;
    profile.crypt = entry.crossuo_crypt;
    profile.product_url = entry.crossuo_producturl;
    profile.web = entry.crossuo_shardweb;
    profile.account_page = entry.crossuo_shardaccount;
    profile.other = entry.crossuo_shardother;
    profile.community = entry.crossuo_shardcommunity;
    profile.video = entry.crossuo_shardvideo;
    profile.seed_override = entry.crossuo_seedoverride;
    profile.system_check_override = entry.crossuo_systemcheckoverride;
    // "" = the profile does not constrain the assistant (keep the client's
    // own value); "no" forbids it (a profile-level Assistant=yes is treated
    // as unconstrained: it is the default everywhere)
    profile.assistant = entry.crossuo_assistant ? "" : "no";
    profile.language = entry.crossuo_language;
    profile.report_language = entry.crossuo_reportlanguage;

    const auto result = xuol::client_apply_shard(profile);
    // needs_restart already latched the reload, which the app frame performs;
    // do NOT request a close in that case: hiding the launcher would stop the
    // frame from running and the pending reload would never be acted upon.
    // The relaunched client takes this process down anyway.
    if (result == xuol::apply_result::applied)
    {
        LOG_INFO("shard applied: %s", profile.name.c_str());
        xuol_launch_quit();
    }
    else if (result == xuol::apply_result::needs_restart)
    {
        LOG_INFO("shard saved, the client reloads to apply it");
        xuol_request_reload();
    }
#else
    // the shard names the launch configuration, mirroring the per-shard
    // naming of account profiles
    const auto cfg = profile_cfg_path(shard.name);
    auto fp = fs_open(cfg, FS_WRITE);
    if (!fp)
    {
        LOG_ERROR("failed to write shard launch configuration");
        return;
    }
    LOG_DEBUG("writing shard launch configuration in: %s", fs_path_ascii(cfg));
    crossuo::write(fp, entry, nullptr);
    fclose(fp);

    auto bin = xuo_client_bin();
    if (fs_path_empty(bin))
    {
        LOG_ERROR("could not find executable: %s", XUO_EXE);
        return;
    }
    fs_path_change(fs_directory(bin));

    const char *args[] = { fs_path_ascii(bin), "--config", fs_path_ascii(cfg), 0 };
    LOG_INFO("running %s", args[0]);

    process_s process;
    const auto options = process_option_inherit_environment | process_option_child_detached |
                         process_option_dont_touch_descriptors;
    if (process_create(args, options, &process) != 0)
    {
        LOG_ERROR("could not launch client %s", args[0]);
        return; // the launch failed, keep the launcher open
    }

    LOG_INFO("shard launched: %s", shard.name);
    xuol_launch_quit();
#endif // XUO_LAUNCHER_EMBEDDED
}

static void account_launch(int account_index)
{
    if (account_index == 0)
        return;

    assert(account_index < (int)s_accounts.entries.size());

#if defined(XUO_LAUNCHER_EMBEDDED)
    xuol_set_last_used(account_index);
    const auto result = account_apply_live(account_index);
    // needs_restart already latched the reload, which the app frame performs;
    // do NOT request a close in that case: hiding the launcher would stop the
    // frame from running and the pending reload would never be acted upon.
    // The relaunched client takes this process down anyway.
    if (result == xuol::apply_result::applied)
    {
        xuol_launch_quit();
    }
    return;
#else
    const auto &entry = s_accounts.entries[account_index];
    auto cfg = account_create_config(entry);
    if (!fs_path_some(cfg))
    {
        LOG_ERROR("error trying to launch selected profile");
        return;
    }

    fs_path xuo_bin = xuo_client_bin();

    auto bin = xuo_bin;
    if (fs_path_empty(bin))
    {
        LOG_ERROR("could not find executable: %s", fs_path_ascii(bin));
        return; // keep the launcher open: cannot launch without a client binary
    }
    fs_path_change(fs_directory(bin));

    const char *args[] = { fs_path_ascii(bin), "--config", fs_path_ascii(cfg), 0 };
    LOG_INFO("running %s", args[0]);

    process_s process;
    const auto options = process_option_inherit_environment | process_option_child_detached |
                         process_option_dont_touch_descriptors;
    if (process_create(args, options, &process) != 0)
    {
        LOG_ERROR("could not launch client %s", args[0]);
        return; // the launch failed, keep the launcher open
    }

    xuol_set_last_used(account_index);
    xuol_launch_quit();
#endif // XUO_LAUNCHER_EMBEDDED
}

static const char *account_getter(void *data, int idx)
{
    auto *items = (std::vector<account::entry> *)data;
    assert(items);
    assert(idx < (int)items->size());
    return items->at(idx).account_profile.c_str();
}

static client_info info;

void ui_accounts(ui_model &m)
{
    const auto profile_max_w = 120.f;
    const auto line_size = ImGui::GetTextLineHeightWithSpacing();
    static auto label_size = ImGui::CalcTextSize("Profile Name: ", nullptr, true);
    static auto label_size2 = ImGui::CalcTextSize(" Use Character: ", nullptr, true);
    // The left column stacks the account list and the Launch button, so the
    // list has to leave room for the button, the spacing after the list and the
    // list's own frame padding. The old "area / (line+2) - 2" heuristic predates
    // the tabs and left the button floating well above the bottom edge.
    const float button_h = ImGui::GetFrameHeight();
    const float list_padding = ImGui::GetStyle().FramePadding.y * 2.0f;
    const float list_spacing = ImGui::GetStyle().ItemSpacing.y;
    int items = int((m.area.y - button_h - list_spacing - list_padding) / line_size);
    if (items < 1)
        items = 1;

    const int NEW_ACCOUNT = 0;
    const int last_item = xuol_last_used();
    static int acct_id = last_item;

    static char profileName[64] = {};
    static char loginServer[128] = {};
    static char login[64] = {};
    static char password[64] = {};
    static char path[FS_MAX_PATH] = {};
    static char characterName[64] = {};
    static char clientVersion[32] = {};
    //static char protocolVersion[32] = {};
    //static char commandLine[FS_MAX_PATH] = {};
    static int current_type = 0;
    static int current_shard = 0;
    static bool useCrypto = false;
    static bool update_view = true;
    static bool ask_help = false;
    const auto picked = shard_picked();
    if (picked != -1)
        update_view = true;

    auto update_shard = [](int id) {
        auto shard = shard_by_id(id);
        memccpy(loginServer, shard.loginserver, 0, sizeof(loginServer));
        memccpy(clientVersion, shard.clientversion, 0, sizeof(clientVersion));
        current_type = account_client_type_index_by_cfg(shard.clienttype);
    };

    ImGuiWindowFlags window_flags = 0;
    float left_w = fmin(floor(m.area.x * (1.0f / 3.0f)), profile_max_w);
    ImGui::BeginChild("left", { left_w, m.area.y }, false, window_flags);
    {
        ImGui::SetNextItemWidth(left_w);
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_SelectedEntryBg));
        if (ListBox(
                "##acct",
                &acct_id,
                account_getter,
                &s_accounts.entries,
                int(s_accounts.entries.size()),
                items,
                &account_launch))
        {
            update_view = true;
        }
        if (acct_id != NEW_ACCOUNT && ImGui::BeginPopupContextItem("##acct"))
        {
            if (ImGui::Selectable("Copy"))
            {
                auto entry = s_accounts.entries[acct_id];
                s_accounts.entries.emplace_back(entry);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Selectable("Delete"))
            {
                s_accounts.entries.erase(s_accounts.entries.begin() + acct_id);
                acct_id--;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Selectable("Export"))
            {
                account_create_config(s_accounts.entries[acct_id]);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Selectable("Launch"))
            {
                account_launch(acct_id);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor();
    }
    if (ImGui::Button("Launch") && acct_id != NEW_ACCOUNT)
    {
        account_launch(acct_id);
    }
    ImGui::EndChild();
    ImGui::SameLine();

    auto &acct = s_accounts.entries[acct_id];
    if (update_view)
    {
        if (acct_id == NEW_ACCOUNT)
        {
            memset(profileName, 0, sizeof(profileName));
            memset(loginServer, 0, sizeof(loginServer));
            memset(login, 0, sizeof(login));
            memset(password, 0, sizeof(password));
            memset(path, 0, sizeof(path));
            memset(characterName, 0, sizeof(characterName));
            memset(clientVersion, 0, sizeof(clientVersion));
            //memset(commandLine, 0, sizeof(commandLine));
            useCrypto = false;
            current_type = 0;
            current_shard = 0;
        }
        else
        {
            memccpy(profileName, acct.account_profile.c_str(), 0, sizeof(profileName));
            memccpy(loginServer, acct.account_loginserver.c_str(), 0, sizeof(loginServer));
            memccpy(login, acct.account_login.c_str(), 0, sizeof(login));
            memccpy(password, acct.account_password.c_str(), 0, sizeof(password));
            memccpy(path, acct.account_data_path.c_str(), 0, sizeof(path));
            memccpy(characterName, acct.account_fast_login.c_str(), 0, sizeof(characterName));
            memccpy(clientVersion, acct.account_clientversion.c_str(), 0, sizeof(clientVersion));
            //memccpy(commandLine, acct.account_extra_cli.c_str(), 0, sizeof(commandLine));
            useCrypto = acct.account_crypt;
            current_type = account_client_type_index_by_cfg(acct.account_clienttype.c_str());
            current_shard = shard_index_by_loginserver(acct.account_loginserver.c_str());
        }

        if (picked != -1)
        {
            LOG_TRACE("picked shard: %d", picked);
            current_shard = picked;
            update_shard(picked);
        }

        update_view = false;
    }

    const float right_w = m.area.x - left_w;
    auto w = right_w;
    ImGui::BeginChild("right", { w, m.area.y }, false);
    const auto spacing_w = 10;
    w = w - spacing_w - label_size.x;
    {
        ImGui::NewLine();
        InputText("##1", "Profile Name:", w, profileName, sizeof(profileName));
        InputText("##2", "       Login:", w / 3, login, sizeof(login));
        ImGui::SameLine();
        const auto w2 = w - ImGui::GetCursorPosX() + 21;
        InputText(
            "##3", " Password:", w2, password, sizeof(password), ImGuiInputTextFlags_Password);
        InputText("##6", "   Character:", w - 22, characterName, sizeof(characterName));
        ImGui::SameLine();
        HelpMarker("If used, will make crossuo fast join the game with this character");
        if (ui_shards_combo("##b", "       Shard:", w - 27, &current_shard))
        {
            update_shard(current_shard);
        }
        ImGui::SameLine();
        if (ImGui::Button("" ICON_FK_GLOBE_W))
            ui_push(m, ui_view::shard_picker);
        HoverToolTip("Shard picker");
        InputText("##5", "UO Data Path:", w - 50, path, sizeof(path));
        ImGui::SameLine();
        if (ImGui::Button("" ICON_FK_FOLDER))
        {
            const auto data_path = tinyfd_selectFolderDialog("UO Data Path", path);
            if (data_path)
            {
                memcpy(path, data_path, sizeof(path));
                const auto client_exe = fs_path_join(data_path, "client.exe");
                const bool client_exe_exists = fs_path_exists(client_exe);
                if (client_exe_exists)
                {
                    ask_help = client_version(fs_path_ascii(client_exe), info) == 0;
                    if (info.version)
                    {
                        client_version_string(info.version, clientVersion, sizeof(clientVersion));
                    }
                    else if (info.xxh3 && info.crc32)
                    {
                        ask_help = true;
                    }
                }
            }
        }

        if (ask_help)
        {
            bool response = false;
            auto closed = ui_modal(
                "Unknown Client Version",
                "X:UO Launcher couldn't auto-detect the client version to use.\n"
                "Would you like to help us improve our launcher?",
                response);
            if (closed)
            {
                if (response)
                {
                    char signature[128];
                    snprintf(
                        signature,
                        sizeof(signature),
                        "0x%016" PRIx64 ", 0x%08x, 0x%08x",
                        info.xxh3,
                        info.crc32,
                        info.version);

                    std::string title = "[xuolauncher] unknown client: ";
                    title += signature;
                    title = http_urlencode(title);

                    std::string body =
                        "My client version is: (put here if you know)\n"
                        "(please, upload your client.exe with a screenshot and link here to help further)";
                    body = http_urlencode(body);

                    std::string url = "https://github.com/crossuo/crossuo/issues/new?title=";
                    url += title + "\\&body=" + body;
                    open_url(url);
                }
                ask_help = false;
            }
        }

        ImGui::SameLine();
        HelpMarker("The place where Ultima Online client is installed");

        // Accounts is already the advanced tab: the former "Advanced" tree node
        // is inlined so Login Server / Client Version / Type / Use Cryptography
        // are always visible. The width juggling it used to carry is kept so the
        // right-column rows keep their previous (fully expanded) widths.
        w = w - label_size2.x + label_size.x;

        InputText("##4", "  Login Server:", w - 27, loginServer, sizeof(loginServer));
        ImGui::SameLine();
        if (ImGui::Button("" ICON_FK_GLOBE_W))
            ui_push(m, ui_view::shard_picker);
        HoverToolTip("Shard picker");
        InputText(
            "##7", "Client Version:", w / 3 - spacing_w, clientVersion, sizeof(clientVersion));
        ImGui::SameLine();
        HelpMarker("Client version to emulate");
        ImGui::SameLine();
        //const auto w3 = w - ImGui::GetCursorPosX() + 14;
        //InputText("##8", " Protocol:", w3, protocolVersion, sizeof(protocolVersion));
        //ImGui::SameLine(); HelpMarker("Network protocol version to emulate (if empty, this will use be the same as client version)");
        //InputText("##9", "  Type:", w3, clientType, sizeof(clientType));
        const auto w3 = right_w - ImGui::GetCursorPosX() - 66;
        ComboBox("##9", "  Type:", w3, &current_type, client_types, IM_ARRAYSIZE(client_types));
        //InputText("##a", "  Command Line:", w, commandLine, sizeof(commandLine));
        ImGui::Checkbox("Use Cryptography", &useCrypto);

        ImGui::NewLine();
        ImGui::SameLine(right_w - 45.0f);
        if (ImGui::Button("Save"))
        {
            if (acct_id == NEW_ACCOUNT)
            {
                if (profileName[0])
                {
                    account::entry entry;
                    entry.account_profile = astr_t(profileName);
                    entry.account_loginserver = astr_t(loginServer);
                    entry.account_login = astr_t(login);
                    entry.account_password = astr_t(password);
                    entry.account_data_path = astr_t(path);
                    entry.account_fast_login = astr_t(characterName);
                    entry.account_clientversion = astr_t(clientVersion);
                    //entry.account_extra_cli = astr_t(commandLine);
                    entry.account_crypt = useCrypto;
                    entry.account_clienttype = astr_t(client_types_cfg[current_type]);
                    acct_id = int(s_accounts.entries.size());
                    s_accounts.entries.emplace_back(entry);
                }
            }
            else
            {
                // dont overwrite the object, we may not want touch other fields
                acct.account_profile = astr_t(profileName);
                acct.account_loginserver = astr_t(loginServer);
                acct.account_login = astr_t(login);
                acct.account_password = astr_t(password);
                acct.account_data_path = astr_t(path);
                acct.account_fast_login = astr_t(characterName);
                acct.account_clientversion = astr_t(clientVersion);
                //acct.account_extra_cli = astr_t(commandLine);
                acct.account_crypt = useCrypto;
                acct.account_clienttype = astr_t(client_types_cfg[current_type]);
            }

            save_config(); // persist all account configuration changes to disk

#if defined(XUO_LAUNCHER_EMBEDDED)
            // saving may have fixed the data path (or anything else); apply
            // the profile live so the waiting client picks it up
            if (acct_id != NEW_ACCOUNT && acct_id < (int)s_accounts.entries.size())
            {
                account_apply_live(acct_id);
            }
#endif
        }
    }
    ImGui::EndChild();
}

} // namespace xuol
