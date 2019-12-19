// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein
#define CFG_DEBUG(...) // LOG_DEBUG(__VA_ARGS__)
#define CFG_TRACE(...) // LOG_TRACE(__VA_ARGS__)

/* clang-format off */
#ifndef CFG_NAME
#error "CFG_NAME not defined"
#define CFG_NAME "error"
#endif

#define CFG_STRINGIFY(X) CFG_STRINGIFY2(X)
#define CFG_STRINGIFY2(X) #X

#ifndef CFG_SECTION_FILTER_NAME
#define CFG_SECTION_FILTER_NAME CFG_STRINGIFY(CFG_NAME)
#endif // CFG_SECTION_FILTER_NAME

namespace CFG_NAME {

#ifndef CFG_CONVERTERS
#include "cfg_converters.h"
#endif

struct entry
{
    #define CFG_FIELD(s, n, default, t) t s##_##n = {}; astr_t raw_##s##_##n = default; bool set_##s##_##n = false;
    #include CFG_DEFINITION
};

entry default_entry()
{
    entry v;

    #define CFG_FIELD(s, n, default, t) convert(v.s##_##n, v.raw_##s##_##n.c_str());
    #include CFG_DEFINITION

    return v;
}

struct data
{
    std::vector<astr_t> errors;
    std::vector<entry> entries;
    entry current = default_entry();
    int lineno = 0;
    bool dirty = false;
    bool strict = true;
    bool ignore_empty_on_save = false;
    bool error = false;
};

void type_save_current(data &data)
{
    if (!data.dirty)
        return;

    CFG_TRACE("saving entry");
    data.entries.emplace_back(data.current);
    data.current = default_entry();
    data.dirty = false;
}

void write(FILE *fp, const entry &e, const char *section)
{
    assert(fp && "invalid fp");
    if (section)
        fprintf(fp, "[%s]\n", section);
    const auto d = default_entry();
    static const char *expected = CFG_SECTION_FILTER_NAME;
    #define CFG_FIELD(s, n, default, t) \
    {\
        if (!section || !expected || strcasecmp(expected, section) == 0)\
        {\
            auto v = as_str(e.s##_##n);\
            if (d.raw_##s##_##n != v || e.set_##s##_##n) \
            {\
                CFG_TRACE("saving %s: '%s' == '%s' (%d)", CFG_STRINGIFY2(n), v.c_str(), d.raw_##s##_##n.c_str(), e.set_##s##_##n);\
                fprintf(fp, "%s=%s\n", CFG_STRINGIFY2(n), v.c_str());\
            }\
        }\
    }
    #include CFG_DEFINITION
    fprintf(fp, "\n");
}

int type_loader(void* user, const char* section, const char* name, const char* value, int lineno)
{
    assert(user && "parser user state not provided");
    auto &obj = *(data *)user;
    obj.lineno = lineno;

    if (strcasecmp(CFG_SECTION_FILTER_NAME, section) != 0)
        return 1; // ignore unknown section

    if (!name && !value)
    {
        type_save_current(obj);
        CFG_TRACE("new section found");
        return 1;
    }

    auto &entry = obj.current;
    CFG_TRACE("%d: %s.%s = %s", lineno, section, name, value);
    if (!name || strlen(name) == 0)
    {
        obj.errors.push_back(std::to_string(lineno) + ": invalid entry");
        obj.error = true;
        return 0;
    }

    bool found = false;
    if (0) ;
#define CFG_FIELD(s, n, default, t) \
    else if (strcasecmp(section, #s) == 0 && strcasecmp(name, #n) == 0) {\
        obj.error |= !convert(entry.s##_##n, value); \
        entry.raw_##s##_##n = value; \
        entry.set_##s##_##n = !(obj.ignore_empty_on_save && (!value || !value[0])); \
        found = true; \
    }
#include CFG_DEFINITION

    if (obj.strict && !found)
    {
        obj.errors.push_back(std::to_string(lineno) + ": unknown entry '" + name + "'");
        obj.error = true;
        return 0;
    }

    obj.dirty = true;
    return 1;
}

void dump(entry *entry)
{
    #define CFG_FIELD(s, n, default, t) CFG_DEBUG("%s_%s = %s", #s, #n, entry->raw_##s##_##n.c_str());
    #include CFG_DEFINITION
}

void cfg(FILE *fp, data &obj)
{
    if (fp && ini_parse_file(fp, type_loader, &obj))
    {
        for (auto &e : obj.errors)
            LOG_ERROR(CFG_SECTION_FILTER_NAME ":%s", e.c_str());
        LOG_ERROR("errors found parsing %s", CFG_SECTION_FILTER_NAME);
    }
    type_save_current(obj);
}

};

#undef TYPE_NAME
#undef CFG_DEFINITION
#undef CFG_STRINGIFY2
#undef CFG_STRINGIFY
#undef CFG_SECTION_FILTER_NAME
#undef CFG_NAME
#undef CFG_CONVERTERS
#undef CFG_DEBUG
#undef CFG_TRACE