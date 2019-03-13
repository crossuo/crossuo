// MIT License

#ifndef WISP_MAPPED_H
#define WISP_MAPPED_H

#include "WispDataStream.h"

namespace Wisp
{
struct CMappedFile : public Wisp::CDataReader
{
    CMappedFile() = default;
    virtual ~CMappedFile();

    bool Load(const os_path &path);
    void Unload();
};

}; // namespace Wisp

#endif // WISP_MAPPED_H
