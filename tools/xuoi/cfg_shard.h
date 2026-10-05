// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

/* clang-format off */
#ifndef CFG_FIELD
#error "x-macros, please define a CFG_FIELD macro on how to interpret the values"
#define CFG_FIELD(section, name, default, type)
#endif

CFG_FIELD(shard, name, {}, astr_t)
// required
// shard name

CFG_FIELD(shard, servertype, {}, astr_t)
// required
// format:
//   <RunUO|ServUO|POL|Sphere> <version number|commit hash>
// server type should contain a well formated server software name and version
// this is useful for crossuo dev team and bug fixing, so MUST be filled.

CFG_FIELD(shard, language, "English", lang_type)
// optional, default: English<,French...>

CFG_FIELD(shard, description, {}, astr_t)
// optional
// format:
//   <short textual description in server official language>

CFG_FIELD(shard, url, {}, astr_t)
// optional
// website url

CFG_FIELD(shard, urlforum, {}, astr_t)
// optional
// website forum url

CFG_FIELD(shard, urlother, {}, astr_t)
// optional
// plain community url, eg. a discord invite
// format:
//   <url>
//   https://discord.gg/INVITE
// no name prefix anymore: a discord link is detected from the url itself
// (contains discord.gg/ or discord.com/)

CFG_FIELD(shard, urlregister, {}, astr_t)
// optional
// url where to register IF the shard is not auto account creation
// if empty, it IS considered auto account creation

CFG_FIELD(shard, installer, {}, astr_t)
// optional
// data install source: where the client streams/downloads its data from
// format:
//   <url>
//   http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo/manifest/
//   empty for manual-install shards: no streaming product, the client falls
//     back to its configured default data
//   a plain http(s) url pointing at a data product: the official uo patch
//     server manifest root/.prod or an aops patch server root
// no type prefixes anymore (uo+/xuo+/website+/download+ are gone): the
//   client deduces the product style from the url content
//   (--streaming-product, xuocore/vfs_source_aops.cpp vfs_aops_product_url)

CFG_FIELD(shard, loginserver, {}, astr_t)
// required
// same as crossuop.cfg, classic loginserver for login.cfg
// format:
//   <ip>,<port>

CFG_FIELD(shard, clientversion, {}, astr_t)
// required
// same as in crossuo.cfg, specify the client version to emulate with crossuo

CFG_FIELD(shard, protocolversion, {}, astr_t)
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

CFG_FIELD(shard, tags, "pvp", tag_data)
// optional, default: pvp
// format:
//   <pvp|pvm|rp>[,...]
// tags to use during filtering, will be mostly curated and moderated.
// some as 'pvp' OR 'pvm' will be enforced

#undef CFG_FIELD
