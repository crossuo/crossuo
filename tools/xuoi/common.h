// MIT License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

#include <external/xxhash.h>
#include <external/lookup3.h>
#include <external/tinyxml2.h>
#include <external/miniz.h>

#define LOG_NEWLINE
#include <common/log.h>
#include <common/fs.h>
#include <common/str.h> // strcasecmp
#include <common/utils.h>
#include <common/checksum.h>

bool valid_url(const std::string &url);
void open_url(const std::string &url);
