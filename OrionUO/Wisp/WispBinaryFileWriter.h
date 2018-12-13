// MIT License

#pragma once

#include "FileSystem.h"

namespace Wisp
{
class CBinaryFileWriter : public Wisp::CDataWriter
{
private:
    FILE *m_File{ nullptr };

public:
    CBinaryFileWriter();

    virtual ~CBinaryFileWriter();

    bool Open(const os_path &path);

    void Close();

    void WriteBuffer();
};

}; // namespace Wisp
