// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define CFG_CONVERTERS

static astr_t join(const std::vector<astr_t> &in, char delim)
{
    if (in.empty())
        return {};
    astr_t r = in[0];
    for (size_t i = 1; i < in.size(); ++i)
        r += delim + in[i];
    return r;
}

static std::vector<astr_t> split(const astr_t &s, char delim)
{
    std::vector<astr_t> r;
    std::istringstream f(s);
    astr_t p;
    while (std::getline(f, p, delim))
        r.push_back(p);
    return r;
}

astr_t as_str(const astr_t &in)
{
    return in;
}

astr_t as_str(const char *in)
{
    return in;
}

bool convert(astr_t &out, const char *raw)
{
    out = raw;
    return true;
}

astr_t as_str(const bool in)
{
    return in ? "yes" : "no";
}

bool convert(int &out, const char *raw)
{
    out = atoi(raw);
    return true;
}

astr_t as_str(const int in)
{
    return std::to_string(in);
}

bool convert(bool &out, const char *raw)
{
    out = false;
    if (!raw || !raw[0])
        return true;

    out = (!strcasecmp("yes", raw) || !strcasecmp("true", raw));
    return true;
}

astr_t as_str(const std::vector<astr_t> &in)
{
    return join(in, '+');
}

bool convert(std::vector<astr_t> &out, const char *raw, char delim = '+')
{
    out = split(raw, delim);
    return true;
}
