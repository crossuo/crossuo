// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#define CFG_CONVERTERS

static std::string join(const std::vector<std::string> &in, char delim)
{
    if (in.empty())
        return {};
    std::string r = in[0];
    for (int i = 1; i < in.size(); ++i)
        r += delim + in[i];
    return r;
}

static std::vector<std::string> split(const std::string &s, char delim)
{
    std::vector<std::string> r;
    std::istringstream f(s);
    std::string p;
    while (std::getline(f, p, delim))
        r.push_back(p);
    return r;
}

std::string as_str(const std::string &in)
{
    return in;
}

std::string as_str(const char *in)
{
    return in;
}

bool convert(std::string &out, const char *raw)
{
    out = raw;
    return true;
}

std::string as_str(const bool in)
{
    return in ? "yes" : "no";
}

bool convert(bool &out, const char *raw)
{
    out = false;
    if (!raw || !raw[0])
        return true;

    out = (!strcasecmp("yes", raw) || !strcasecmp("true", raw));
    return true;
}

std::string as_str(const std::vector<std::string> &in)
{
    return join(in, '+');
}

bool convert(std::vector<std::string> &out, const char *raw, char delim = '+')
{
    out = split(raw, delim);
    return true;
}
