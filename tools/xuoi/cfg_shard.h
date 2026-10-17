// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

/* clang-format off */
#ifndef CFG_FIELD
#error "x-macros, please define a CFG_FIELD macro on how to interpret the values"
#define CFG_FIELD(section, name, default, type)
#endif

// the [Shard] grammar mirrors the [crossuo] keys (cfg_crossuo.h): a shard
// entry is directly usable as a crossuo config - shared keys are read by the
// client, launcher-only keys (description, autoacct, tags) are accepted and
// ignored by the client's config parser

CFG_FIELD(shard, shardname, {}, astr_t)
// required
// shard name; names the client's per-shard data directory

CFG_FIELD(shard, language, {}, astr_t)
// optional
// the default client locale (.loc) to use, e.g. English

CFG_FIELD(shard, reportlanguage, {}, astr_t)
// optional
// same as crossuo.cfg, the language code reported to the server

CFG_FIELD(shard, seedoverride, {}, astr_t)
// optional
// same as crossuo.cfg, replaces the IP-derived login seed
// format: <0xaa> <0xbb> <0xcc> <0xdd>

CFG_FIELD(shard, systemcheckoverride, {}, astr_t)
// optional
// same as crossuo.cfg, payload of the client->server 0x03 talk acknowledge
// format: <0xaa> <0xbb> ...

CFG_FIELD(shard, assistant, "yes", bool)
// optional, default: yes (assumed when the field is absent)
// format:
//   <no|yes>
// same as crossuo.cfg; 'no' forbids the client from launching its assistant
// on this shard
CFG_FIELD(shard, description, {}, astr_t)
// optional, launcher-only
// short textual description in the shard's official language

CFG_FIELD(shard, shardweb, {}, astr_t)
// optional, http(s)
// shard website

CFG_FIELD(shard, shardcommunity, {}, astr_t)
// optional, http(s)
// forum/community page url

CFG_FIELD(shard, shardother, {}, astr_t)
// optional, http(s)
// plain community url, eg. a discord invite
// format:
//   <url>
//   https://discord.gg/INVITE
// no name prefix: a discord link is detected from the url itself
// (contains discord.gg/ or discord.com/)

CFG_FIELD(shard, shardaccount, {}, astr_t)
// optional, http(s)
// url where to register/manage accounts; independent of autoacct: a shard
// may be autoacct=yes and still link an account page

CFG_FIELD(shard, shardvideo, {}, astr_t)
// optional, plain http(s) url — shard video/trailer; drives the client's
// video link (crossuo.cfg 'shardvideo')

CFG_FIELD(shard, producturl, {}, astr_t)
// optional
// data install source: where the client streams/downloads its data from
// format:
//   <url>
//   http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo/manifest/
//   empty for manual-install shards: no streaming product, the client falls
//     back to its configured default data
//   a plain http(s) url pointing at a data product: the official uo patch
//     server manifest root/.prod or an aops patch server root
//   a folder on the WEB PROXY host: "local:<path>" (absolute, or relative to
//     the dist tree). xuoproxy auto-mounts it and rewrites the served
//     shards.cfg to a same-origin product url; a raw install folder is
//     presented to clients as a generated MythicMFT product (an existing
//     manifest on disk is served as-is). Web only - the desktop launcher
//     does not mount local: installers
// no type prefixes (uo+/xuo+/website+/download+ are gone): the client
//   deduces the product style from the url content
//   (--streaming-product, xuocore/vfs_source_aops.cpp vfs_aops_product_url)

CFG_FIELD(shard, loginserver, {}, astr_t)
// required
// same as crossuo.cfg, classic loginserver for login.cfg
// format:
//   <ip>,<port>

CFG_FIELD(shard, clientversion, {}, astr_t)
// required
// same as in crossuo.cfg, specify the client version to emulate with crossuo

CFG_FIELD(shard, protocolclientversion, {}, astr_t)
// optional
// same as in crossuo.cfg, specify the network protocol version to force

CFG_FIELD(shard, clienttype, {}, astr_t)
// optional
// one of: t2a,re,td,lbr,aos,se,sa,ml,ej
// same as crossuo.cfg, if empty will autodetect based on the client version

CFG_FIELD(shard, crypt, "no", bool)
// otpional, default: no
// format:
//   <no|yes>
// if 'yes', uo protocol cryptography will be used

CFG_FIELD(shard, autoacct, "yes", bool)
// optional, launcher-only, default: yes (assumed when the field is absent)
// format:
//   <no|yes>
// whether the shard creates the game account on first login. With 'no' the
// clients must offer a way to reach the registration page (shardaccount):
// the desktop launcher's row menu and the web picker's card both grow a
// "Create account" action.

CFG_FIELD(shard, tags, "pvp", tag_data)
// optional, launcher-only, default: pvp
// format:
//   <pvp|pvm|rp>[,...]
// tags to use during filtering, will be mostly curated and moderated.
// some as 'pvp' OR 'pvm' will be enforced

#undef CFG_FIELD
