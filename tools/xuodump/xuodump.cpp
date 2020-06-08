// AGPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include <vector>
#include <inttypes.h>
#include <external/popts.h>
#include <external/cbase64.h>
#include <external/zlib_amalg.h>

#include <common/fs.h>
#include <common/str.h>
#include <common/checksum.h>

#include <xuocore/uodata.h>
#include <openssl/sha.h>

static po::parser s_cli;
static bool s_manifest = true; // s_cli["manifest"]

static std::vector<uint8_t> compress_data(std::vector<uint8_t> data)
{
    const auto srcLen = data.size();
    auto dstLen = srcLen;
    std::vector<uint8_t> dst;
    dst.resize(dstLen);
    auto src = reinterpret_cast<unsigned char const *>(data.data());
    auto p = reinterpret_cast<unsigned char *>(dst.data());
    int z_err = compress(p, &dstLen, src, srcLen);
    if (z_err != Z_OK)
    {
        Error(Data, "compression failed %d", z_err);
        return data;
    }
    dst.resize(dstLen);
    return dst;
}

size_t compress_file(const fs_path &infile, const fs_path &outfile, size_t *ul)
{
    size_t data_len = 0;
    auto data = fs_map(infile, &data_len);
    auto out_len = data_len;
    auto out = (unsigned char *)malloc(out_len);
    int z_err = compress(out, &out_len, data, data_len);
    fs_unmap(data, data_len);
    assert(z_err == Z_OK);
    auto zfp = fs_open(outfile, fs_mode::FS_WRITE);
    assert(zfp);
    fwrite(out, out_len, 1, zfp);
    fclose(zfp);
    free(out);

    if (ul)
        *ul = data_len;
    return out_len;
}

// mftw - manifest writer

struct mftw_entry
{
    astr_t name;
    size_t uncompressed_len = 0;
    size_t compressed_len = 0;
    uint64_t timestamp = 0;
    uint32_t ph = 0;
    uint32_t sh = 0;
    uint32_t compression_type = 0;
    uint32_t hash = 0;
    uint32_t ft = 0;
    uint32_t meta_len = 0;
    uint32_t meta_crc = 0;
    uint32_t sig_type = 0;
    astr_t sig; // sha256 base64 string
};

struct mftw_package : mftw_entry
{
    astr_t rpath;
    std::vector<mftw_entry> parts;
    bool packed = true;
};

struct mftw_manifest : mftw_entry
{
    std::vector<mftw_package> packages;
};

struct mftw_product
{
    mftw_manifest pkg;
};

void mftw_package_init(mftw_package &package, const char *name);
void mftw_package_add(
    mftw_package &package,
    uint32_t ph,
    uint32_t sh,
    size_t ul,
    size_t cl,
    uint32_t ml,
    uint32_t mc,
    uint32_t t,
    uint32_t rh);
void mftw_package_write(mftw_package &package);
void mftw_manifest_init(mftw_manifest &manifest, const char *name);
void mftw_manifest_add(mftw_manifest &manifest, mftw_package package);
void mftw_manifest_write(mftw_manifest &manifest);

void mftw_manifest_init(mftw_manifest &manifest, const char *name)
{
    manifest.name = name;
    manifest.packages = {};
}

void mftw_manifest_add(mftw_manifest &manifest, mftw_package package)
{
    manifest.packages.emplace_back(package);
}

void mftw_manifest_write(mftw_manifest &manifest)
{
    const char *hdr = R"mft(<MythicMFT>
    <manifest>
        <manifests>)mft";
    const char *bdy = R"mft(
            <manifest n="%s.mft" ul="%x" ct="%u" cl="%x" t="%x" rh="%u">
                <sig id="%u" v="%s" />
            </manifest>)mft";
    const char *ftr = R"mft(
        </manifests>
    </manifest>
</MythicMFT>)mft";

    std::sort(manifest.packages.begin(), manifest.packages.end(), [](auto &a, auto &b) -> bool {
        return a.timestamp < b.timestamp;
    });
    char rawname[FS_MAX_PATH];
    snprintf(rawname, sizeof(rawname), "%s.mft.raw", manifest.name.c_str());
    auto fp = fs_open(fs_path_from(rawname), fs_mode::FS_WRITE);
    assert(fp);
    fprintf(fp, "%s", hdr);
    for (auto &entry : manifest.packages)
    {
        if (!entry.timestamp)
            entry.timestamp = (uint64_t)time(0);
        fprintf(
            fp,
            bdy,
            entry.name.c_str(),
            entry.uncompressed_len,
            entry.compression_type,
            entry.compressed_len,
            entry.timestamp,
            entry.hash,
            entry.sig_type,
            entry.sig.c_str());
    }
    fprintf(fp, "%s", ftr);
    fs_close(fp);

    char zname[FS_MAX_PATH];
    snprintf(zname, sizeof(zname), "%s.mft", manifest.name.c_str());
    size_t ul = 0;
    size_t cl = compress_file(fs_path_from(rawname), fs_path_from(zname), &ul);

    manifest.uncompressed_len = ul;
    manifest.compressed_len = cl;
    manifest.compression_type = ul != cl ? 1 : 0;
    manifest.sig_type = 21;
    manifest.sig = "base64"; // FIXME
    manifest.timestamp = (uint64_t)time(0);
}

void mftw_package_init(mftw_package &package, const char *name)
{
    package.name = name;
    package.rpath = name;
    package.parts = {};
}

void mftw_package_add(
    mftw_package &package,
    uint32_t ph,
    uint32_t sh,
    size_t ul,
    size_t cl,
    uint32_t ml,
    uint32_t mc,
    uint32_t t,
    uint32_t rh)
{
    mftw_entry entry;
    entry.uncompressed_len = ul;
    entry.compressed_len = cl;
    entry.compression_type = ul != cl ? 1 : 0;
    entry.ph = ph;
    entry.sh = sh;
    entry.meta_len = ml;
    entry.meta_crc = mc; // FIXME
    entry.timestamp = t;
    entry.hash = rh; // FIXME
    package.parts.emplace_back(entry);
}

void mftw_package_write(mftw_package &package)
{
    const char *hdr = R"mft(<MythicMFT>
    <manifest>
        <packs>
            <p name="%s" rpath="%s">
                <files>)mft";
    const char *bdy = R"mft(
                    <f ph="%x" sh="%x" ul="%x" ct="%u" cl="%x" ft="%u" ml="%x" mc="%x" t="%x" rh="%x" />)mft";
    const char *ftr = R"mft(
                </files>
            </p>
        </packs>
    </manifest>
</MythicMFT>)mft";

    std::sort(package.parts.begin(), package.parts.end(), [](auto &a, auto &b) -> bool {
        if (a.ph == b.ph)
            return a.sh < b.sh;
        return a.ph < b.ph;
    });
    char rawname[FS_MAX_PATH];
    snprintf(rawname, sizeof(rawname), "%s.mft.raw", package.rpath.c_str());
    auto fp = fs_open(fs_path_from(rawname), fs_mode::FS_WRITE);
    assert(fp);
    fprintf(fp, hdr, package.name.c_str(), package.rpath.c_str());
    for (auto &entry : package.parts)
    {
        if (!entry.timestamp)
            entry.timestamp = (uint64_t)time(0);
        fprintf(
            fp,
            bdy,
            entry.ph,
            entry.sh,
            entry.uncompressed_len,
            entry.compression_type,
            entry.compressed_len,
            entry.ft,
            entry.meta_len,
            entry.meta_crc,
            entry.timestamp,
            entry.hash);
    }
    fprintf(fp, "%s", ftr);
    fs_close(fp);

    char zname[FS_MAX_PATH];
    snprintf(zname, sizeof(zname), "%s.mft", package.rpath.c_str());
    size_t ul = 0;
    size_t cl = compress_file(fs_path_from(rawname), fs_path_from(zname), &ul);

    package.uncompressed_len = ul;
    package.compressed_len = cl;
    package.compression_type = ul != cl ? 1 : 0;
    package.sig_type = 21;
    package.sig = "base64"; // FIXME
    package.timestamp = (uint64_t)time(0);
}

// main dumper

static mftw_manifest mft;
static mftw_product prod;

static bool init_cli(int argc, char *argv[])
{
    s_cli["help"].abbreviation('h').description("print this help screen").callback([&] {
        std::cout << s_cli << '\n';
    });

    s_cli["file"].abbreviation('f').type(po::string).description("input file");
    s_cli["info"].abbreviation('i').description("output info");
    s_cli["unpack"].abbreviation('u').description("unpack (file)");
    s_cli["manifest"].abbreviation('m').description("generate manifest");
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

void uop_unpack_file(CUopMappedFile &file, astr_t filename)
{
    cbase64_encodestate b64;
    cbase64_init_encodestate(&b64);

    auto p = fs_path_from(filename);
    fs_path_create(p);

    mftw_package pkg;
    mftw_package_init(pkg, filename.c_str());

    for (auto &entry : file.m_MapByHash)
    {
        auto asset = file.GetAsset(entry.first);
        auto data = file.GetRaw(asset);
        auto meta = file.GetMeta(asset);

        char fname[100];
        snprintf(
            fname, sizeof(fname), "%08x%08x", UOP_HASH_SH(asset->Hash), UOP_HASH_PH(asset->Hash));
        char mname[100];
        snprintf(mname, sizeof(mname), "%s.meta", fname);

        auto f = fs_path_join(p, fname);
        if (asset->Flags == 0) // not compressed, we need to compress
        {
            data = compress_data(data);
        }
        fs_file_write(f, data);
        f = fs_path_join(p, mname);
        fs_file_write(f, meta);

        auto meta_crc = adler32(meta.data(), meta.size());
        auto data_crc = adler32(data.data(), data.size());
        mftw_package_add(
            pkg,
            UOP_HASH_SH(asset->Hash),
            UOP_HASH_PH(asset->Hash),
            asset->DecompressedSize,
            data.size(),
            meta.size(),
            meta_crc,
            0,
            data_crc);
    }

    if (s_manifest)
        mftw_package_write(pkg);
    mftw_manifest_add(mft, pkg);
}

int main(int argc, char **argv)
{
    if (!init_cli(argc, argv))
    {
        return 0;
    }
    crc32_init();
    auto uopath = cli("client_path");
    auto unpack = s_cli["unpack"].was_set();
    auto info = s_cli["info"].was_set();
    auto filename = cli("file");
    const auto path = fs_path_from(uopath);
    fs_case_insensitive_init(path);

    mftw_manifest_init(mft, "pkg");

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
        CUopMappedFile file, x;
        g_FileManager.UopLoadFile(file, filename.c_str(), info);
        Info(Data, "data loaded.");

        if (unpack)
        {
            uop_unpack_file(file, filename);
        }
    }

    if (s_manifest)
        mftw_manifest_write(mft);
    /*
    CDataReader reader;
    std::vector<uint8_t> tempData;
    bool isUOP = false;

    if ( g_FileManager.m_GumpartLegacyMUL.Start != nullptr)
    {
    }

    if (g_FileManager.m_MainMisc.Start != nullptr)
    {
        auto block = g_FileManager.m_MainMisc.GetAsset(
            Asset::CommandsTranslate);
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

void test_base64()
{
    const auto data =
        "AIEB0VP09QvXgJgdJoxU62Y/c8XJDHaxgINoKK77NAJxiPHx71WfUvOqWU6MPkQhfdwbX2CNgmy3Vb+7yPswmbwSlGoPz29obVQz2Cge9H3Yxf/YVfMksZVk7SNhjAjPyDpxsHI/WYKqZpH8Wjmi9hiXi3xLRcn5MqkqrXviYLn2v08=";
    char out2[1024] = {};
    unsigned char out[1024] = {};
    int s = 0;
    {
        cbase64_decodestate b64;
        cbase64_init_decodestate(&b64);
        s = cbase64_decode_block(data, strlen(data), out, &b64);
        for (int i = 0; i < s; i++)
        {
            fprintf(stdout, "%02x ", out[i]);
        }
    }
    {
        cbase64_encodestate b64;
        cbase64_init_encodestate(&b64);
        int l = cbase64_encode_block(out, s, out2, &b64);
        l += cbase64_encode_blockend(&out2[l], &b64);
        fprintf(stdout, "\n%s\n", out2);
        assert(l == strlen(data));
        assert(strcmp(out2, data) == 0);
    }
}