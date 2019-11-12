// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

struct xuo_context;
struct xuo_release;

#define XUOL_VERSION "1.1"
#define XUOL_AGENT_NAME "X:UO Launcher v" XUOL_VERSION
#define XUOL_UPDATER_HOST "http://update.crossuo.com/"
//#define XUOL_UPDATER_HOST "http://192.168.2.14:8089/"

typedef void (*xuo_release_cb)(const char *release, const char *version, bool latest);

xuo_context *xuo_init(const char *path, bool beta = false);
const char *xuo_platform_name();
const char *xuo_changelog(xuo_context *ctx);
void xuo_releases_iterate(xuo_context *ctx, xuo_release_cb pFunc);
bool xuo_release_get(xuo_context *ctx, const char *name, const char *version = nullptr);
bool xuo_update_check(xuo_context *ctx);
bool xuo_update_apply(xuo_context *ctx);
void xuo_shutdown(xuo_context *ctx);
