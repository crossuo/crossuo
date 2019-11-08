// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

struct xuo_context;

#define XUOL_VERSION "1.1"
#define XUOL_AGENT_NAME "X:UO Launcher v" XUOL_VERSION
#define XUOL_UPDATER_HOST "http://update.crossuo.com/"
//#define XUOL_UPDATER_HOST "http://192.168.2.14:8089/"

xuo_context *xuo_init(const char *path, bool beta = false);
const char *xuo_platform_name();
const char *xuo_changelog(xuo_context *ctx);
bool xuo_update_check(xuo_context *ctx);
bool xuo_update_apply(xuo_context *ctx);
void xuo_shutdown(xuo_context *ctx);
