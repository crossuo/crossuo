// MIT License

#pragma once

#include "FileSystem.h"

namespace Wisp
{
class CBinaryFileWritter : public Wisp::CDataWritter
{
private:
    FILE *m_File{ nullptr };

public:
    CBinaryFileWritter();

    virtual ~CBinaryFileWritter();

    bool Open(const os_path &path);

    void Close();

    void WriteBuffer();
};

};
