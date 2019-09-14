// MIT License

#ifndef WISP_BFW_H
#define WISP_BFW_H

#include <common/fs.h>
#include <xuocore/mappedfile.h>

namespace Wisp
{
class CBinaryFileWriter : public CDataWriter
{
private:
    FILE *m_File{ nullptr };

public:
    CBinaryFileWriter();

    virtual ~CBinaryFileWriter();

    bool Open(const fs_path &path);

    void Close();

    void WriteBuffer();
};

}; // namespace Wisp

#endif // WISP_BFW_H
