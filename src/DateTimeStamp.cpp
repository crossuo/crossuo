// MIT License
// Copyright (C) September 2017 Hotride

#include "DateTimeStamp.h"

#if defined(XUO_WINDOWS)

astr_t GetBuildDateTimeStamp()
{
    return astr_t(__DATE__);
}

#else

#include "GitRevision.h"
astr_t GetBuildDateTimeStamp()
{
    return astr_t(__DATE__ " " __TIME__ " (" GIT_REV_STR ")");
}

#endif
