// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "WispBinaryFileWriter.h"

namespace Wisp
{
CBinaryFileWriter::CBinaryFileWriter()

{
}

CBinaryFileWriter::~CBinaryFileWriter()
{
    Close();
}

bool CBinaryFileWriter::Open(const fs_path &path)
{
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
    WriteBuffer();

    if (m_File != nullptr)
    {
        fs_close(m_File);
        m_File = nullptr;
    }
}

void CBinaryFileWriter::WriteBuffer()
{
    if (m_File != nullptr && !m_Data.empty())
    {
        fwrite(&m_Data[0], m_Data.size(), 1, m_File);
        m_Data.clear();
    }
}

}; // namespace Wisp
