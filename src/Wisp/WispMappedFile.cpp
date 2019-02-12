#include "WispMappedFile.h"
#include "../FileSystem.h"

namespace Wisp
{
CMappedFile::CMappedFile()

{
}

CMappedFile::~CMappedFile()
{
    Unload();
}

bool CMappedFile::Load(const os_path &path)
{
    DEBUG_TRACE_FUNCTION;
    Info(Filesystem, "mmaping %s", CStringFromPath(path));
    bool result = false;

    if (fs_path_exists(path))
    {
        Unload();
        Start = fs_map(path, &Size);
        result = Start != nullptr;
        SetData(Start, Size);
    }
    else
    {
        Warning(Filesystem, "file not found %s", CStringFromPath(path));
    }

    if (!result)
    {
        auto errorCode = errno;
        Error(Filesystem, "failed to memory map, error code: %i", errorCode);
    }

    return result;
}

void CMappedFile::Unload()
{
    //DEBUG_TRACE_FUNCTION;
    if (Start != nullptr)
    {
        fs_unmap(Start, Size);
    }
    SetData(nullptr, 0);
}

}; // namespace Wisp
