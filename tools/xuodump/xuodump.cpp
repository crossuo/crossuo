#include <string>
#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include <vector>
#include <external/popts.h>

void uo_log(int type, const char *sys, const char *fmt, ...);
void dump_hex(int type, const char *sys, const char *title, uint8_t *buf, int size);
#define log(...) uo_log(0, "xuodump", __VA_ARGS__)
#define FS_LOG_DEBUG(...) log(__VA_ARGS__)
#define FS_LOG_ERROR(...) log(__VA_ARGS__)
#include <common/fs.h>
#include <common/str.h>

#include <xuocore/uodata.h>

static po::parser s_cli;
static bool s_log = false;

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

void dump_hex(int type, const char *sys, const char *title, uint8_t *buf, int size)
{
    if (title && title[0] != '\0')
    {
        uo_log(type, sys, "%s", title);
    }
    int num_lines = size / 16;
    if (size % 16 != 0)
    {
        num_lines++;
    }

    for (int line = 0; line < num_lines; line++)
    {
        char out[128];
        char *cur = out;
        char *end = cur + sizeof(out);
        int row = 0;
        int r = 0;
        r = snprintf(cur, end - cur, "%04X: ", line * 16);
        assert(r >= 0 && r < end - cur && end - cur >= 0);
        cur += r;
        for (row = 0; row < 16; row++)
        {
            if (line * 16 + row < size)
            {
                r = snprintf(cur, end - cur, "%02X ", buf[line * 16 + row]);
                assert(r >= 0 && r < end - cur && end - cur >= 0);
                cur += r;
            }
            else
            {
                r = snprintf(cur, end - cur, "-- ");
                assert(r >= 0 && r < end - cur && end - cur >= 0);
                cur += r;
            }
        }
        r = snprintf(cur, end - cur, ": ");
        assert(r >= 0 && r < end - cur && end - cur >= 0);
        cur += r;
        for (row = 0; row < 16; row++)
        {
            if (line * 16 + row < size)
            {
                const char ch = isprint(buf[line * 16 + row]) != 0 ? buf[line * 16 + row] : '.';
                r = snprintf(cur, end - cur, ch == '%' ? "%c%%" : "%c", ch);
                assert(r >= 0);
                assert(r < end - cur);
                assert(end - cur >= 0);
                cur += r;
            }
        }
        uo_log(type, sys, "%s", out);
    }
}

static bool init_cli(int argc, char *argv[])
{
    s_cli["help"].abbreviation('h').description("print this help screen").callback([&] {
        std::cout << s_cli << '\n';
    });

    s_cli["file"].abbreviation('f').type(po::string).description("input file");
    s_cli["info"].abbreviation('i').description("output info");
    s_cli["unpack"].abbreviation('u').description("unpack (file)");
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

    auto uopath = cli("client_path");
    auto unpack = s_cli["unpack"].was_set();
    auto info = s_cli["info"].was_set();
    auto filename = cli("file");

    const auto path = fs_path_from(uopath);
    fs_case_insensitive_init(path);

    // TOOD: remove
    auto version = cli("client_version", "7.0.15.0");
    uo_data_init(uopath.c_str(), VERSION(7, 0, 15, 0), false);

    log("client path: %s", uopath.c_str());
    log("client version: %s", version.c_str());
    log("loading data...");
    g_dumpUopFile = filename;
    if (filename.empty())
    {
        g_FileManager.Load();
        log("data loaded.");
    }
    else
    {
        CUopMappedFile file;
        g_FileManager.UopLoadFile(file, filename.c_str(), info);
        log("data loaded.");

        if (unpack)
        {
            auto p = fs_path_from(filename);
            fs_path_create(p);

            for (auto &entry : file.m_MapByName)
            {
                auto asset = file.GetAsset(entry.first);
                auto data = file.GetRaw(asset);
                auto meta = file.GetMeta(asset);

                char fname[100];
                snprintf(fname, sizeof(fname), "%016lx", asset->Hash);
                char mname[100];
                snprintf(mname, sizeof(mname), "%016lx.meta", asset->Hash);

                auto f = fs_path_join(p, fname);
                fs_file_write(f, data);
                f = fs_path_join(p, mname);
                fs_file_write(f, meta);
            }
        }
    }

    /*
    CDataReader reader;
    std::vector<uint8_t> tempData;
    bool isUOP = false;

    if ( g_FileManager.m_GumpartLegacyMUL.Start != nullptr)
    {
    }

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
*/
    log("unloading data...");
    g_FileManager.Unload();
    log("data unloaded.");

    return 0;
}
