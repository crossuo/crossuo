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
    TRACE(Filesystem, "mmaping: {}", CStringFromPath(path));
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
        DEBUG(Filesystem, "file not found: {}", CStringFromPath(path));
    }

    if (!result)
    {
        auto errorCode = errno;
        DEBUG(
            Filesystem,
            "failed to memory map file {} with error code: {}",
            CStringFromPath(path),
            errorCode);
    }

    return result;
}

void CMappedFile::Unload()
{
    DEBUG_TRACE_FUNCTION;
    if (Start != nullptr)
    {
        fs_unmap(Start, Size);
    }
    SetData(nullptr, 0);
}

}; // namespace Wisp
