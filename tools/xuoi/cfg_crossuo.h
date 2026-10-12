// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

/* clang-format off */
#ifndef CFG_FIELD
#error "x-macros, please define a CFG_FIELD macro on how to interpret the values"
#define CFG_FIELD(section, name, default, type)
#endif

CFG_FIELD(crossuo, acctid, {}, astr_t)
CFG_FIELD(crossuo, acctpassword, {}, astr_t)
CFG_FIELD(crossuo, rememberacctpw, "no", bool)
CFG_FIELD(crossuo, autologin, "no", bool)
CFG_FIELD(crossuo, theabyss, "no", bool)
CFG_FIELD(crossuo, asmut, "no", bool)
CFG_FIELD(crossuo, custompath, {}, astr_t)
CFG_FIELD(crossuo, loginserver, {}, astr_t)
CFG_FIELD(crossuo, clientversion, {}, astr_t)
CFG_FIELD(crossuo, protocolclientversion, {}, astr_t)
CFG_FIELD(crossuo, crypt, "no", bool)
CFG_FIELD(crossuo, useverdata, "no", bool)
CFG_FIELD(crossuo, clienttype, {}, astr_t)
CFG_FIELD(crossuo, language, {}, astr_t)
CFG_FIELD(crossuo, shardname, {}, astr_t)
// shard presentation links: written by the launcher's shard picker only when
// the shard provides them (an explicit empty value would clear the client
// default); keys match src/Config.cpp s_Keys
CFG_FIELD(crossuo, producturl, {}, astr_t)
CFG_FIELD(crossuo, shardweb, {}, astr_t)
CFG_FIELD(crossuo, shardaccount, {}, astr_t)
CFG_FIELD(crossuo, shardhelp, {}, astr_t)
CFG_FIELD(crossuo, shardvideo, {}, astr_t)
CFG_FIELD(crossuo, shardcredits, {}, astr_t)
CFG_FIELD(crossuo, shardother, {}, astr_t)
CFG_FIELD(crossuo, shardcommunity, {}, astr_t)

#undef CFG_FIELD
