// MIT License

#ifndef WISP_MAPPED_H
#define WISP_MAPPED_H

#include "WispDataStream.h"

namespace Wisp
{
class CMappedFile : public Wisp::CDataReader
{
public:
    CMappedFile();
    virtual ~CMappedFile();

    bool Load(const os_path &path);
    void Unload();
};

}; // namespace Wisp

#endif // WISP_MAPPED_H
