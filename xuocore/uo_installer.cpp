// AGPLv3 License
// Copyright (c) 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Installer

#include "mft.h"
#include "http.h"
#include "common.h"
#include "xuoi.h"
#include <inttypes.h>
#include <thread>

#define DATA_PRODUCT_SERVER "http://patch.uo.eamythic.com/"
#define DATA_PRODUCT_PATH "uopatch-sa/legacyrelease/uo/manifest/"
#define DATA_PRODUCT_URL DATA_PRODUCT_SERVER DATA_PRODUCT_PATH
#define DATA_PRODUCT_FILE "uo-legacyrelease.prod"
#define UOI_AGENT_NAME "EAMythic Patch Client"
#define UOI_MAX_DOWNLOAD_SIZE (1024 * 1024 * 1024)

bool uoi_load_contents()
{
    fs_path cache_path = fs_path_join(fs_path_appdata(), "xuolauncher", "uo");
    if (!fs_path_create(cache_path))
    {
        LOG_ERROR("failed to create cache directory: %s", fs_path_ascii(cache_path));
        return false;
    }
    LOG_INFO("Cache path: %s", fs_path_ascii(cache_path));

    mft_config cfg;
    cfg.agent_name = UOI_AGENT_NAME;
    cfg.download_buffer_size = UOI_MAX_DOWNLOAD_SIZE;
    cfg.thread_count = std::thread::hardware_concurrency() - 1;
    cfg.listing_only = true;
    cfg.dump_extracted = false;
    cfg.output_path = cache_path;

    mft_result res = xuoi_product_install(cfg, DATA_PRODUCT_URL, DATA_PRODUCT_FILE);
    return res == mft_ok;
}
