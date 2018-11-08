// MIT License
// Copyright (C) September 2017 Hotride

#if !defined(ORION_LINUX)

string GetBuildDateTimeStamp()
{
    return string(__DATE__);
}

#else

#include "GitRevision.h"
string GetBuildDateTimeStamp()
{
    return string(__DATE__ " " __TIME__ " (" GIT_REV_STR ")");
}

#endif
