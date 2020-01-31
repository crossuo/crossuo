// MIT License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

#include <common/fs.h>
#include <common/str.h> // strcasecmp
#include <common/utils.h>
#include <common/checksum.h>

#define LOG_INFO(...) Info(Launcher, __VA_ARGS__)
#define LOG_TRACE(...) TRACE(Launcher, __VA_ARGS__)
#define LOG_DEBUG(...) DEBUG(Launcher, __VA_ARGS__)
#define LOG_WARN(...) Warning(Launcher, __VA_ARGS__)
#define LOG_ERROR(...) Error(Launcher, __VA_ARGS__)

bool valid_url(const astr_t &url);
void open_url(const astr_t &url);
