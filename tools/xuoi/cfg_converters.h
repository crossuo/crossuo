bool convert(std::string &out, const char *raw)
{
    out = raw;
    return true;
}

bool convert(bool &out, const char *raw)
{
    out = false;
    if (!raw || !raw[0])
        return true;

    out = (!strcasecmp("yes", raw) || !strcasecmp("true", raw));
    return true;
}
