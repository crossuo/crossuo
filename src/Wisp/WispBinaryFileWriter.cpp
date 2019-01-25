// MIT License

namespace Wisp
{
CBinaryFileWriter::CBinaryFileWriter()

{
}

CBinaryFileWriter::~CBinaryFileWriter()
{
    Close();
}

bool CBinaryFileWriter::Open(const os_path &path)
{
    DEBUG_TRACE_FUNCTION;
    bool result = false;

    //if (fs_path_exists(path))
    {
        m_File = fs_open(path, FS_WRITE);
        result = (m_File != nullptr);
    }

    return result;
}

void CBinaryFileWriter::Close()
{
    DEBUG_TRACE_FUNCTION;
    WriteBuffer();

    if (m_File != nullptr)
    {
        fs_close(m_File);
        m_File = nullptr;
    }
}

void CBinaryFileWriter::WriteBuffer()
{
    DEBUG_TRACE_FUNCTION;
    if (m_File != nullptr && !m_Data.empty())
    {
        fwrite(&m_Data[0], m_Data.size(), 1, m_File);
        m_Data.clear();
    }
}

}; // namespace Wisp
