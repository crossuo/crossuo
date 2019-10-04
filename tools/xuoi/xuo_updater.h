// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

struct xuo_context;

xuo_context *xuo_init(const char *path, bool beta);
const char *xuo_platform_name();
const char *xuo_changelog(xuo_context *ctx);
bool xuo_update_check(xuo_context *ctx);
bool xuo_update_apply(xuo_context *ctx);
void xuo_shutdown(xuo_context *ctx);
