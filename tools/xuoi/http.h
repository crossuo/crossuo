// MIT License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

void http_init();
void http_shutdown();
void http_set_user_agent(const char *name);
void http_get_binary(const char *url, const uint8_t *buf, size_t *size);
void http_get_binary(const char *url, std::vector<uint8_t> &data);
void http_get_string(const char *url, std::string &data);
bool http_get_file(const char *url, const char *filename);