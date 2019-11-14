// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

/* clang-format off */
#ifndef CFG_FIELD
#error "x-macros, please define a CFG_FIELD macro on how to interpret the values"
#define CFG_FIELD(section, name, default, type)
#endif

CFG_FIELD(crossuo, acctid, {}, std::string)
CFG_FIELD(crossuo, acctpassword, {}, std::string)
CFG_FIELD(crossuo, rememberacctpw, "no", bool)
CFG_FIELD(crossuo, autologin, "no", bool)
CFG_FIELD(crossuo, theabyss, "no", bool)
CFG_FIELD(crossuo, asmut, "no", bool)
CFG_FIELD(crossuo, custompath, {}, std::string)
CFG_FIELD(crossuo, loginserver, {}, std::string)
CFG_FIELD(crossuo, clientversion, {}, std::string)
CFG_FIELD(crossuo, protocolclientversion, {}, std::string)
CFG_FIELD(crossuo, crypt, "no", bool)
CFG_FIELD(crossuo, useverdata, "no", bool)
CFG_FIELD(crossuo, clienttype, {}, std::string)
CFG_FIELD(crossuo, language, {}, std::string)

#undef CFG_FIELD
