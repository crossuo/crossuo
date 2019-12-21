// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

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

#undef CFG_FIELD
