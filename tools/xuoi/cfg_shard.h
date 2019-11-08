// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

/* clang-format off */
#ifndef CFG_FIELD
#error "x-macros, please define a CFG_FIELD macro on how to interpret the values"
#define CFG_FIELD(section, name, default, type)
#endif

CFG_FIELD(shard, submitter, "github+crossuo", std::string)
// default: github+crossuo
// submitter name / contact
// format:
//   github+<username> expands to https://github.com/<username>
//   otherwise a string for name with optional <email@address>

CFG_FIELD(shard, name, {}, std::string)
// required
// shard name

CFG_FIELD(shard, servertype, {}, std::string)
// required
// format:
//   <RunUO|ServUO|POL|Sphere> <version number|commit hash>
// server type should contain a well formated server software name and version
// this is useful for crossuo dev team and bug fixing, so MUST be filled.

CFG_FIELD(shard, language, "ENU", lang_type)
// optional, default: ENU (English)
// format:
//   <uo lang code>
//     ENU,FRA,...
// same as in crossuo.cfg, used for filtering

CFG_FIELD(shard, description, {}, std::string)
// optional
// format:
//   <short textual description in server official language>

CFG_FIELD(shard, url, {}, std::string)
// required
// website url

CFG_FIELD(shard, urlforum, {}, std::string)
// optional
// website forum url

CFG_FIELD(shard, urlother, {}, url_other)
// optional
// other url, eg. community chat or anything
// format:
//   <textual description>+<url>
//   Discord+https://discord.gg/INVITE

CFG_FIELD(shard, urlregister, {}, std::string)
// optional
// url where to register IF the shard is not auto account creation
// if empty, it IS considered auto account creation

CFG_FIELD(shard, installer, {}, std::string)
// optional
// install command/url/type
// format:
//   download+<type>+<url>
//     this will download the thing in the url and if <type> is 'zip' it will
//     unpack upon download. if <type> is 'exe' it will execute directly.
//   uo+<latest|url>
//     this uses the same official uo patching system, latest will use the
//     official and latest uo.com data, if <url> is used, then this will try
//     to use this as entry point for a uo compatible patching data (can be 
//     created with xuoi --mirror mode for example.
//   xuo+<url>
//     this uses the X:UO update server and manifest format

CFG_FIELD(shard, loginserver, {}, std::string)
// required
// same as crossuop.cfg, classic loginserver for login.cfg
// format:
//   <ip>,<port>

CFG_FIELD(shard, clientversion, {}, std::string)
// required
// same as in crossuo.cfg, specify the client version to emulate with crossuo

CFG_FIELD(account, protocolversion, {}, std::string)
// required
// same as in crossuo.cfg, specify the network protocol version to force

CFG_FIELD(shard, clienttype, {}, std::string)
// optional
// same as crossuo.cfg, if empty will autodetect based on the client version

CFG_FIELD(shard, crypto, "no", std::string)
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
