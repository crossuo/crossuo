// MIT License

#ifndef WISP_BFW_H
#define WISP_BFW_H

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

    bool Open(const os_path &path);

    void Close();

    void WriteBuffer();
};

}; // namespace Wisp

#endif // WISP_BFW_H
