// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <common/fs.h>
#include <common/str.h> // strcasecmp
#include <common/utils.h>
#include <common/checksum.h>

#define LOG_INFO(...) Info(LOGGER_MODULE, __VA_ARGS__)
#define LOG_TRACE(...) TRACE(LOGGER_MODULE, __VA_ARGS__)
#define LOG_DEBUG(...) DEBUG(LOGGER_MODULE, __VA_ARGS__)
#define LOG_WARN(...) Warning(LOGGER_MODULE, __VA_ARGS__)
#define LOG_ERROR(...) Error(LOGGER_MODULE, __VA_ARGS__)

bool valid_url(const astr_t &url);
void open_url(const astr_t &url);
