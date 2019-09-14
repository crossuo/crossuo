#include <string>
#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include <vector>

#include <common/str.h>
#include <common/fs.h>

#include <external/popts.h>
#include <xuocore/uodata.h>

static po::parser s_cli;
static bool s_log = false;

#define log(...) uo_log(0, "xuodump", __VA_ARGS__)
void uo_log(int type, const char *sys, const char *fmt, ...);

#define FS_LOG_DEBUG(...) log(__VA_ARGS__)
#define FS_LOG_ERROR(...) log(__VA_ARGS__)

void uo_log(int type, const char *sys, const char *fmt...)
{
    if (!s_log)
        return;

    fprintf(stdout, "[%s] ", sys);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fprintf(stdout, "\n");
}

static bool init_cli(int argc, char *argv[])
{
    s_cli["help"].abbreviation('h').description("print this help screen").callback([&] {
        std::cout << s_cli << '\n';
    });

    s_cli["dump"].abbreviation('d').type(po::string).description("dump file to csv");
    s_cli["client_path"].abbreviation('p').type(po::string).description("uo data path");
    s_cli["client_version"]
        .abbreviation('c')
        .type(po::string)
        .description("uo client version string");
    s_cli["log"].abbreviation('l').description("enable logging").callback([&] { s_log = true; });
    s_cli(argc, argv);

    return s_cli["help"].size() == 0;
}

static std::string cli(const char *arg, const char *val = "")
{
    return s_cli[arg].was_set() ? s_cli[arg].get().string : val;
}

int main(int argc, char **argv)
{
    if (!init_cli(argc, argv))
    {
        return 0;
    }

    auto dump_file = cli("dump");
    auto uopath = cli("client_path");
    auto version = cli("client_version", "7.0.15.0");

    log("client path: %s", uopath.c_str());
    log("client version: %s", version.c_str());

    const auto path = fs_path_from(uopath);
    fs_case_insensitive_init(path);
    uo_data_init(uopath.c_str(), VERSION(7, 0, 15, 0), false);

    log("loading data...");
    g_FileManager.Load();
    log("data loaded.");

    CDataReader reader;
    std::vector<uint8_t> tempData;
    bool isUOP = false;
    if (g_FileManager.m_MainMisc.Start != nullptr)
    {
        auto block = g_FileManager.m_MainMisc.GetBlock(
            0x0891F809004D8081); // FIXME: find the string for this hash
        if (block != nullptr)
        {
            tempData = g_FileManager.m_MainMisc.GetData(block);
            reader.SetData(&tempData[0], tempData.size());
            isUOP = true;
        }
    }

    if (reader.Start == nullptr)
    {
        reader.SetData(g_FileManager.m_SpeechMul.Start, g_FileManager.m_SpeechMul.Size);
    }

    if (isUOP)
    {
        log("loading speech from UOP");
        reader.Move(2);
        std::wstring mainData = reader.ReadWStringLE(reader.Size - 2);
        std::wcout << mainData << std::endl;
    }

    log("unloading data...");
    g_FileManager.Unload();
    log("data unloaded.");

    return 0;
}
