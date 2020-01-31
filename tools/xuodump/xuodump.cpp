// AGPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include <vector>
#include <inttypes.h>
#include <external/popts.h>

#include <common/fs.h>
#include <common/str.h>

#include <xuocore/uodata.h>

static po::parser s_cli;

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
    s_cli["log"].abbreviation('l').description("enable logging").callback([&] {
        g_LogEnabled = eLogSystem::LogSystemAll;
    });
    s_cli(argc, argv);

    return s_cli["help"].size() == 0;
}

static astr_t cli(const char *arg, const char *val = "")
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

    Info(Data, "client path: %s", uopath.c_str());
    Info(Data, "client version: %s", version.c_str());
    Info(Data, "loading data...");
    g_dumpUopFile = filename;
    if (filename.empty())
    {
        g_FileManager.Load();
        Info(Data, "data loaded.");
    }
    else
    {
        CUopMappedFile file;
        g_FileManager.UopLoadFile(file, filename.c_str(), info);
        Info(Data, "data loaded.");

        if (unpack)
        {
            auto p = fs_path_from(filename);
            fs_path_create(p);

            for (auto &entry : file.m_MapByHash)
            {
                auto asset = file.GetAsset(entry.first);
                auto data = file.GetRaw(asset);
                auto meta = file.GetMeta(asset);

                char fname[100];
                snprintf(fname, sizeof(fname), "%016" PRIx64, asset->Hash);
                char mname[100];
                snprintf(mname, sizeof(mname), "%016" PRIx64 ".meta", asset->Hash);

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
        Info(Data, "loading speech from UOP");
        reader.Move(2);
        wstr_t mainData = reader.ReadWStringLE(reader.Size - 2);
        std::wcout << mainData << std::endl;
    }
*/
    Info(Data, "unloading data...");
    g_FileManager.Unload();
    Info(Data, "data unloaded.");

    return 0;
}
