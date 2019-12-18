// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include "accounts.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <external/inih.h>
#include <external/process.h>
#include <external/tinyfiledialogs.h>
#include "common.h"
#include "ui_model.h"
#include "shards.h"
#include "ui_shards.h"

#define CFG_NAME account
#define CFG_DEFINITION "cfg_launcher.h"
#include "cfg_loader.h"

#define CFG_NAME crossuo
#define CFG_DEFINITION "cfg_crossuo.h"
#include "cfg_loader.h"

#if defined(XUO_WINDOWS)
#define XUO_EXE "crossuo.exe"
#else
#define XUO_EXE "./crossuo"
#endif

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
    "", "t2a", "re", "td", "lbr", "aos", "se", "sa", "ml", "ej",
};

static account::data s_accounts;
static std::unordered_map<std::string, int> s_account_by_name;

void load_accounts()
{
    const auto fname = fs_path_join(fs_path_current(), "xuolauncher.cfg");
    LOG_INFO("loading accounts from %s", fs_path_ascii(fname));
    auto fp = fs_open(fname, FS_READ);
    account::cfg(fp, s_accounts);
    if (fp)
        fs_close(fp);
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
    for (int i = 1; i < s_accounts.entries.size(); ++i)
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

static fs_path account_create_config(const account::entry &account, bool save = false)
{
    std::string name = account.account_profile + ".cfg";
    std::replace(name.begin(), name.end(), '/', '_');
    std::replace(name.begin(), name.end(), '\\', '_');

    const auto path = save ? fs_path_current() : fs_path_join(fs_appdata_path(), "xuolauncher");
    const auto fname = fs_path_join(path, name);
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
    entry.crossuo_crypt = account.account_crypto;
    entry.crossuo_custompath = account.account_data_path;
    entry.crossuo_loginserver = account.account_loginserver;

    crossuo::write(fp, entry, nullptr);
    fclose(fp);
    return fname;
}

static void account_launch(int account_index)
{
    assert(account_index > 0 && account_index < s_accounts.entries.size());
    const auto &entry = s_accounts.entries[account_index];
    auto cfg = account_create_config(entry);
    if (!fs_path_some(cfg))
    {
        LOG_ERROR("error trying to launch selected profile");
        return;
    }

    const char *args[] = { XUO_EXE, "--config", fs_path_ascii(cfg), 0 };
    LOG_INFO("running %s", args[0]);

    process_s process;
    const auto options = process_option_inherit_environment | process_option_child_detached |
                         process_option_dont_touch_descriptors;
    if (process_create(args, options, &process) != 0)
    {
        LOG_ERROR("could not launch client %s", args[0]);
    }

    void xuol_launch_quit();
    xuol_launch_quit();
}

// view

void HoverToolTip(const char *desc);
void HelpMarker(const char *desc);
void InputText(
    const char *id,
    const char *label,
    float w,
    char *buf,
    size_t buf_size,
    ImGuiInputTextFlags flags = 0,
    ImGuiInputTextCallback callback = nullptr,
    void *user_data = nullptr);
bool ComboBox(
    const char *id,
    const char *label,
    float w,
    int *current_item,
    const char *const items[],
    int items_count,
    int height_in_items = -1);

static bool account_getter(void *data, int idx, const char **out_text)
{
    auto *items = (std::vector<account::entry> *)data;
    assert(items);
    assert(idx < items->size());
    if (out_text)
        *out_text = items->at(idx).account_profile.c_str();
    return true;
}

void ui_accounts(ui_model &m)
{
    const auto profile_max_w = 120.f;
    const auto line_size = ImGui::GetTextLineHeightWithSpacing();
    static auto label_size = ImGui::CalcTextSize("Profile Name: ", nullptr, true);
    static auto label_size2 = ImGui::CalcTextSize(" Use Character: ", nullptr, true);
    const auto items = m.area.y / (line_size + 2) - 2;

    const int NEW_ACCOUNT = 0;
    const int last_item = NEW_ACCOUNT;
    static int acct_id = last_item;

    static char profileName[64] = {};
    static char loginServer[128] = {};
    static char login[64] = {};
    static char password[64] = {};
    static char path[FS_MAX_PATH] = {};
    static char characterName[64] = {};
    static char clientVersion[32] = {};
    //static char protocolVersion[32] = {};
    static char commandLine[FS_MAX_PATH] = {};
    static int current_type = 0;
    static int current_shard = 0;
    static bool update_view = true;
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
        ImGui::Text(ICON_FK_USER " Accounts");
        ImGui::SetNextItemWidth(left_w);
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_SelectedEntryBg));
        if (ImGui::ListBox(
                "##acct",
                &acct_id,
                account_getter,
                &s_accounts.entries,
                int(s_accounts.entries.size()),
                items))
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
                account_create_config(s_accounts.entries[acct_id], true);
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
            memset(commandLine, 0, sizeof(commandLine));
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
            memccpy(commandLine, acct.account_extra_cli.c_str(), 0, sizeof(commandLine));
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
            }
        }
        ImGui::SameLine();
        HelpMarker("The place where Ultima Online client is installed");

        if (ImGui::TreeNode("Advanced"))
        {
            w = w - label_size2.x + label_size.x;
            ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());

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
            InputText("##a", "  Command Line:", w, commandLine, sizeof(commandLine));

            ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
            ImGui::TreePop();
        }
        ImGui::NewLine();
        ImGui::SameLine(right_w - 45.0f);
        if (ImGui::Button("Save"))
        {
            if (acct_id == NEW_ACCOUNT)
            {
                if (profileName[0])
                {
                    account::entry entry;
                    entry.account_profile = std::string(profileName);
                    entry.account_loginserver = std::string(loginServer);
                    entry.account_login = std::string(login);
                    entry.account_password = std::string(password);
                    entry.account_data_path = std::string(path);
                    entry.account_fast_login = std::string(characterName);
                    entry.account_clientversion = std::string(clientVersion);
                    entry.account_extra_cli = std::string(commandLine);
                    entry.account_clienttype = std::string(client_types_cfg[current_type]);
                    acct_id = int(s_accounts.entries.size());
                    s_accounts.entries.emplace_back(entry);
                }
            }
            else
            {
                // dont overwrite the object, we may not want touch other fields
                acct.account_profile = std::string(profileName);
                acct.account_loginserver = std::string(loginServer);
                acct.account_login = std::string(login);
                acct.account_password = std::string(password);
                acct.account_data_path = std::string(path);
                acct.account_fast_login = std::string(characterName);
                acct.account_clientversion = std::string(clientVersion);
                acct.account_extra_cli = std::string(commandLine);
                acct.account_clienttype = std::string(client_types_cfg[current_type]);
            }

            void save_config();
            save_config(); // force xuolauncher to save all configurations
        }
    }
    ImGui::EndChild();
}
