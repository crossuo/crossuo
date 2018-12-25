// MIT License
// Copyright (C) August 2016 Hotride

CFontsManager g_FontManager;

CFontsManager::CFontsManager()
{
    DEBUG_TRACE_FUNCTION;
    memset(m_UnicodeFontAddress, 0, sizeof(m_UnicodeFontAddress));
    memset(m_UnicodeFontSize, 0, sizeof(m_UnicodeFontSize));
}

CFontsManager::~CFontsManager()
{
    DEBUG_TRACE_FUNCTION;
    delete[] Font;
    FontCount = 0;
    m_WebLink.clear();
}

bool CFontsManager::LoadFonts()
{
    DEBUG_TRACE_FUNCTION;

    Wisp::CMappedFile fontFile;

    if (!fontFile.Load(g_App.UOFilesPath("fonts.mul")))
    {
        return false;
    }

    FontCount = 0;

    while (!fontFile.IsEOF())
    {
        bool exit = false;
        fontFile.Move(1);

        for (int i = 0; i < 224; i++)
        {
            FONT_HEADER *fh = (FONT_HEADER *)fontFile.Ptr;
            fontFile.Move(sizeof(FONT_HEADER));

            int bcount = fh->Width * fh->Height * 2;

            if (fontFile.Ptr + bcount > fontFile.End) //Bad font file...
            {
                exit = true;

                break;
            }

            fontFile.Move(bcount);
        }

        if (exit)
        {
            break;
        }

        FontCount++;
    }

    if (FontCount < 1)
    {
        Font = nullptr;
        FontCount = 0;

        return false;
    }

    Font = new FONT_DATA[FontCount];

    fontFile.ResetPtr();

    for (int i = 0; i < FontCount; i++)
    {
        FONT_DATA &fd = Font[i];
        fd.Header = fontFile.ReadUInt8();

        for (int j = 0; j < 224; j++)
        {
            FONT_CHARACTER_DATA &fcd = fd.Chars[j];
            fcd.Width = fontFile.ReadUInt8();
            fcd.Height = fontFile.ReadUInt8();
            fontFile.Move(1);

            int dataSize = fcd.Width * fcd.Height;
            fcd.Data.resize(dataSize);
            fontFile.ReadDataLE((uint8_t *)&fcd.Data[0], dataSize * 2);
        }
    }

    for (int i = 0; i < 20; i++)
    {
        m_UnicodeFontAddress[i] = (size_t)g_FileManager.m_UnifontMul[i].Start;
        m_UnicodeFontSize[i] = (uint32_t)g_FileManager.m_UnifontMul[i].Size;
    }

    if (m_UnicodeFontAddress[1] == 0)
    {
        m_UnicodeFontAddress[1] = m_UnicodeFontAddress[0];
        m_UnicodeFontSize[1] = m_UnicodeFontSize[0];
    }

    for (int i = 0; i < 256; i++)
    {
        if (m_FontIndex[i] >= 0xE0)
        {
            m_FontIndex[i] = m_FontIndex[' '];
        }
    }

    return true;
}

bool CFontsManager::UnicodeFontExists(uint8_t font)
{
    DEBUG_TRACE_FUNCTION;
    return !(font >= 20 || m_UnicodeFontAddress[font] == 0);
}

void CFontsManager::GoToWebLink(uint16_t link)
{
    DEBUG_TRACE_FUNCTION;
    WEBLINK_MAP::iterator it = m_WebLink.find(link);

    if (it != m_WebLink.end())
    {
        it->second.Visited = true;
        g_Orion.GoToWebLink(it->second.WebLink);
    }
}

int CFontsManager::GetFontOffsetY(uint8_t font, uint8_t index)
{
    DEBUG_TRACE_FUNCTION;

    if (index == 0xB8)
    {
        return 1;
    }
    if (!(index >= 0x41 && index <= 0x5A) && !(index >= 0xC0 && index <= 0xDF) && index != 0xA8)
    {
        if (font < 10)
        {
            if (index >= 0x61 && index <= 0x7A)
            {
                const int offsetCharTable[] = { 2, 0, 2, 2, 0, 0, 2, 2, 0, 0 };
                return offsetCharTable[font];
            }

            const int offsetSymbolTable[] = { 1, 0, 1, 1, -1, 0, 1, 1, 0, 0 };
            return offsetSymbolTable[font];
        }

        return 2;
    }

    return 0;
}

Wisp::CPoint2Di CFontsManager::GetCaretPosA(
    uint8_t font, const string &str, int pos, int width, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    Wisp::CPoint2Di p;

    if (font >= FontCount || pos < 1 || str.empty())
    {
        return p;
    }

    FONT_DATA &fd = Font[font];

    if (width == 0)
    {
        width = GetWidthA(font, str);
    }

    PMULTILINES_FONT_INFO info =
        GetInfoA(font, str.c_str(), (int)str.length(), align, flags, width);
    if (info == nullptr)
    {
        return p;
    }

    int height = 0;
    PMULTILINES_FONT_INFO ptr = info;

    //loop throgh lines to get width and height
    while (info != nullptr)
    {
        p.X = 0;
        int len = info->CharCount;
        if (info->CharStart == pos)
        {
            return p;
        }

        //if pos is not in this line, just skip this
        if (pos <= info->CharStart + len && (int)ptr->Data.size() >= len)
        {
            for (int i = 0; i < len; i++)
            {
                //collect data about width of each character
                uint8_t index = m_FontIndex[(uint8_t)ptr->Data[i].item];
                p.X += fd.Chars[index].Width;

                if (info->CharStart + i + 1 == pos)
                {
                    return p;
                }
            }
        }

        //add to height if there's another line
        if (info->m_Next != nullptr)
        {
            p.Y += info->MaxHeight;
        }

        PMULTILINES_FONT_INFO ptr = info;

        info = info->m_Next;

        ptr->Data.clear();
        delete ptr;
    }

    return p;
}

int CFontsManager::CalculateCaretPosA(
    uint8_t font, const string &str, int x, int y, int width, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    if (font >= FontCount || x < 0 || y < 0 || str.empty())
    {
        return 0;
    }

    FONT_DATA &fd = Font[font];

    if (width == 0)
    {
        width = GetWidthA(font, str);
    }

    if (x >= width)
    {
        return (int)str.length();
    }

    PMULTILINES_FONT_INFO info =
        GetInfoA(font, str.c_str(), (int)str.length(), align, flags, width);
    if (info == nullptr)
    {
        return 0;
    }

    int height = GetHeightA(info);

    PMULTILINES_FONT_INFO ptr = info;

    height = 0;

    int pos = 0;
    bool found = false;

    while (ptr != nullptr)
    {
        info = ptr;

        height += ptr->MaxHeight;
        width = 0;

        if (!found)
        {
            if (y < height)
            {
                int len = ptr->CharCount;

                for (int i = 0; i < len; i++)
                {
                    uint8_t index = m_FontIndex[(uint8_t)ptr->Data[i].item];

                    width += fd.Chars[index].Width;

                    if (width > x)
                    {
                        break;
                    }

                    pos++;
                }

                found = true;
            }
            else
            {
                pos += ptr->CharCount;
                pos++;
            }
        }

        ptr = ptr->m_Next;

        info->Data.clear();
        delete info;
    }

    return pos;
}

int CFontsManager::GetWidthA(uint8_t font, const string &str)
{
    DEBUG_TRACE_FUNCTION;
    if (font >= FontCount || str.empty())
    {
        return 0;
    }

    FONT_DATA &fd = Font[font];
    int textLength = 0;

    for (char c : str)
    {
        textLength += fd.Chars[m_FontIndex[(uint8_t)c]].Width;
    }

    return textLength;
}

int CFontsManager::GetWidthExA(
    uint8_t font, const string &str, int maxWidth, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    if (font >= FontCount || str.empty())
    {
        return 0;
    }

    PMULTILINES_FONT_INFO info =
        GetInfoA(font, str.c_str(), (int)str.length(), align, flags, maxWidth);

    int textWidth = 0;

    while (info != nullptr)
    {
        if (info->Width > textWidth)
        {
            textWidth = info->Width;
        }

        PMULTILINES_FONT_INFO ptr = info;
        info = info->m_Next;

        ptr->Data.clear();
        delete ptr;
    }

    return textWidth;
}

int CFontsManager::GetHeightA(
    uint8_t font, const string &str, int width, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    if (font >= FontCount || str.empty())
    {
        return 0;
    }

    if (width == 0)
    {
        width = GetWidthA(font, str);
    }

    PMULTILINES_FONT_INFO info =
        GetInfoA(font, str.c_str(), (int)str.length(), align, flags, width);

    int textHeight = 0;

    while (info != nullptr)
    {
        textHeight += info->MaxHeight;

        PMULTILINES_FONT_INFO ptr = info;
        info = info->m_Next;

        ptr->Data.clear();
        delete ptr;
    }

    return textHeight;
}

int CFontsManager::GetHeightA(PMULTILINES_FONT_INFO info)
{
    DEBUG_TRACE_FUNCTION;
    int textHeight = 0;

    while (info != nullptr)
    {
        textHeight += info->MaxHeight;

        info = info->m_Next;
    }

    return textHeight;
}

string CFontsManager::GetTextByWidthA(uint8_t font, const string &str, int width, bool isCropped)
{
    DEBUG_TRACE_FUNCTION;
    if (font >= FontCount || str.empty())
    {
        return string("");
    }

    FONT_DATA &fd = Font[font];

    if (isCropped)
    {
        width -= fd.Chars[m_FontIndex[(uint8_t)'.']].Width * 3;
    }

    int textLength = 0;
    string result = {};
    for (char c : str)
    {
        textLength += fd.Chars[m_FontIndex[(uint8_t)c]].Width;

        if (textLength > width)
        {
            break;
        }

        result += c;
    }

    if (isCropped)
    {
        result += "...";
    }

    return result;
}

PMULTILINES_FONT_INFO CFontsManager::GetInfoA(
    uint8_t font, const char *str, int len, TEXT_ALIGN_TYPE align, uint16_t flags, int width)
{
    DEBUG_TRACE_FUNCTION;
    if (font >= FontCount)
    {
        return nullptr;
    }

    FONT_DATA &fd = Font[font];

    PMULTILINES_FONT_INFO info = new MULTILINES_FONT_INFO();
    info->Reset();
    info->Align = align;

    PMULTILINES_FONT_INFO ptr = info;

    int indentionOffset = 0;
    ptr->IndentionOffset = 0;

    bool isFixed = (flags & UOFONT_FIXED) != 0;
    bool isCropped = (flags & UOFONT_CROPPED) != 0;

    int charCount = 0;
    int lastSpace = 0;
    int readWidth = 0;

    for (int i = 0; i < len; i++)
    {
        char si = str[i];

        if (si == '\r' || si == '\n')
        {
            if (si == '\r' || isFixed || isCropped)
            {
                continue;
            }
            {
                si = '\n';
            }
        }

        if (si == ' ')
        {
            lastSpace = i;
            ptr->Width += readWidth;
            readWidth = 0;
            ptr->CharCount += charCount;
            charCount = 0;
        }

        FONT_CHARACTER_DATA &fcd = fd.Chars[m_FontIndex[(uint8_t)si]];

        if (si == '\n' || ptr->Width + readWidth + fcd.Width > width)
        {
            if (lastSpace == ptr->CharStart && (lastSpace == 0) && si != '\n')
            {
                ptr->CharStart = 1;
            }

            if (si == '\n')
            {
                ptr->Width += readWidth;
                ptr->CharCount += charCount;

                lastSpace = i;

                if (ptr->Width == 0)
                {
                    ptr->Width = 1;
                }

                if (ptr->MaxHeight == 0)
                {
                    ptr->MaxHeight = 14;
                }

                ptr->Data.resize(ptr->CharCount);

                PMULTILINES_FONT_INFO newptr = new MULTILINES_FONT_INFO();
                newptr->Reset();

                ptr->m_Next = newptr;

                ptr = newptr;

                ptr->Align = align;
                ptr->CharStart = i + 1;

                readWidth = 0;
                charCount = 0;
                indentionOffset = 0;

                ptr->IndentionOffset = 0;

                continue;
            }
            if (lastSpace + 1 == ptr->CharStart && !isFixed && !isCropped)
            {
                ptr->Width += readWidth;
                ptr->CharCount += charCount;

                if (ptr->Width == 0)
                {
                    ptr->Width = 1;
                }

                if (ptr->MaxHeight == 0)
                {
                    ptr->MaxHeight = 14;
                }

                PMULTILINES_FONT_INFO newptr = new MULTILINES_FONT_INFO();
                newptr->Reset();

                ptr->m_Next = newptr;

                ptr = newptr;

                ptr->Align = align;
                ptr->CharStart = i;
                lastSpace = i - 1;
                charCount = 0;

                if (ptr->Align == TS_LEFT && ((flags & UOFONT_INDENTION) != 0))
                {
                    indentionOffset = 14;
                }

                ptr->IndentionOffset = indentionOffset;

                readWidth = indentionOffset;
            }
            else
            {
                if (isFixed)
                {
                    MULTILINES_FONT_DATA mfd = {
                        (uint16_t)si, flags, font, 0, 0xFFFFFFFF, nullptr
                    };
                    ptr->Data.push_back(mfd);

                    readWidth += fcd.Width;

                    if (fcd.Height > ptr->MaxHeight)
                    {
                        ptr->MaxHeight = fcd.Height;
                    }

                    charCount++;

                    ptr->Width += readWidth;
                    ptr->CharCount += charCount;
                }

                i = lastSpace + 1;
                si = str[i];

                if (ptr->Width == 0)
                {
                    ptr->Width = 1;
                }

                if (ptr->MaxHeight == 0)
                {
                    ptr->MaxHeight = 14;
                }

                ptr->Data.resize(ptr->CharCount);
                charCount = 0;

                if (isFixed || isCropped)
                {
                    break;
                }

                PMULTILINES_FONT_INFO newptr = new MULTILINES_FONT_INFO();
                newptr->Reset();

                ptr->m_Next = newptr;

                ptr = newptr;

                ptr->Align = align;
                ptr->CharStart = i;

                if (ptr->Align == TS_LEFT && ((flags & UOFONT_INDENTION) != 0))
                {
                    indentionOffset = 14;
                }

                ptr->IndentionOffset = indentionOffset;

                readWidth = indentionOffset;
            }
        }

        MULTILINES_FONT_DATA mfd = { (uint16_t)si, flags, font, 0, 0xFFFFFFFF, nullptr };
        ptr->Data.push_back(mfd);

        readWidth += fcd.Width;

        if (fcd.Height > ptr->MaxHeight)
        {
            ptr->MaxHeight = fcd.Height;
        }

        charCount++;
    }

    ptr->Width += readWidth;
    ptr->CharCount += charCount;

    if ((readWidth == 0) && (len != 0) && (str[len - 1] == '\n' || str[len - 1] == '\r'))
    {
        ptr->Width = 1;
        ptr->MaxHeight = 14;
    }

    if (font == 4)
    {
        ptr = info;

        while (ptr != nullptr)
        {
            if (ptr->Width > 1)
            {
                ptr->MaxHeight = ptr->MaxHeight + 2;
            }
            else
            {
                ptr->MaxHeight = ptr->MaxHeight + 6;
            }

            ptr = ptr->m_Next;
        }
    }

    return info;
}

bool CFontsManager::GenerateA(
    uint8_t font,
    CGLTextTexture &th,
    const string &str,
    uint16_t color,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    if (((flags & UOFONT_FIXED) != 0) || ((flags & UOFONT_CROPPED) != 0))
    {
        th.Clear();

        if ((width == 0) || str.empty())
        {
            return false;
        }

        int realWidth = GetWidthA(font, str);

        if (realWidth > width)
        {
            string newstr = GetTextByWidthA(font, str, width, (flags & UOFONT_CROPPED) != 0);
            return GenerateABase(font, th, newstr, color, width, align, flags);
        }
    }

    return GenerateABase(font, th, str, color, width, align, flags);
}

vector<uint32_t> CFontsManager::GeneratePixelsA(
    uint8_t font,
    CGLTextTexture &th,
    const char *str,
    uint16_t color,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    vector<uint32_t> pData;

    th.Clear();

    if (font >= FontCount)
    {
        return pData;
    }

    int len = (int)strlen(str);
    if (len == 0)
    {
        return pData;
    }

    FONT_DATA &fd = Font[font];

    if (width == 0)
    {
        width = GetWidthA(font, str);
    }

    if (width == 0)
    {
        return pData;
    }

    PMULTILINES_FONT_INFO info = GetInfoA(font, str, len, align, flags, width);
    if (info == nullptr)
    {
        return pData;
    }

    width += 4;

    int height = GetHeightA(info);

    if (height == 0)
    {
        PMULTILINES_FONT_INFO ptr = info;

        while (ptr != nullptr)
        {
            info = ptr;

            ptr = ptr->m_Next;

            info->Data.clear();
            delete info;
        }

        return pData;
    }

    int blocksize = height * width;

    pData.resize(blocksize, 0);

    int lineOffsY = 0;
    PMULTILINES_FONT_INFO ptr = info;

    bool partialHue = (font != 5 && font != 8) && !UnusePartialHue;
    int font6OffsetY = (int)(font == 6) * 7;

    while (ptr != nullptr)
    {
        info = ptr;

        th.LinesCount = th.LinesCount + 1;

        int w = 0;
        if (ptr->Align == TS_CENTER)
        {
            w = (((width - 10) - ptr->Width) / 2);
            if (w < 0)
            {
                w = 0;
            }
        }
        else if (ptr->Align == TS_RIGHT)
        {
            w = ((width - 10) - ptr->Width);
            if (w < 0)
            {
                w = 0;
            }
        }
        else if (ptr->Align == TS_LEFT && ((flags & UOFONT_INDENTION) != 0))
        {
            w = ptr->IndentionOffset;
        }

        int count = (int)ptr->Data.size();
        for (int i = 0; i < count; i++)
        {
            uint8_t index = (uint8_t)ptr->Data[i].item;
            int offsY = GetFontOffsetY(font, index);

            FONT_CHARACTER_DATA &fcd = fd.Chars[m_FontIndex[index]];
            int dw = fcd.Width;
            int dh = fcd.Height;
            uint16_t charColor = color;
            for (int y = 0; y < dh; y++)
            {
                int testY = y + lineOffsY + offsY;
                if (testY >= height)
                {
                    break;
                }

                for (int x = 0; x < dw; x++)
                {
                    if ((x + w) >= width)
                    {
                        break;
                    }

                    uint16_t pic = fcd.Data[(y * dw) + x];
                    if (pic != 0u)
                    {
                        uint32_t pcl = 0;
                        if (partialHue)
                        {
                            pcl = g_ColorManager.GetPartialHueColor(pic, charColor);
                        }
                        else
                        {
                            pcl = g_ColorManager.GetColor(pic, charColor);
                        }

                        int block = (testY * width) + (x + w);
                        if (block >= 0)
                        {
                            pData[block] = pcl << 8 | 0xFF;
                        }
                    }
                }
            }

            w += dw;
        }

        lineOffsY += (ptr->MaxHeight - font6OffsetY);

        ptr = ptr->m_Next;

        info->Data.clear();
        delete info;
    }

    th.Width = width;
    th.Height = height;

    return pData;
}

bool CFontsManager::GenerateABase(
    uint8_t font,
    CGLTextTexture &th,
    const string &str,
    uint16_t color,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    vector<uint32_t> pixels = GeneratePixelsA(font, th, str.c_str(), color, width, align, flags);
    bool result = false;

    if (static_cast<unsigned int>(!pixels.empty()) != 0u)
    {
        g_GL_BindTexture32(th, th.Width, th.Height, &pixels[0]);

        result = true;
    }

    return true;
}

void CFontsManager::DrawA(
    uint8_t font,
    const string &str,
    uint16_t color,
    int x,
    int y,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    CGLTextTexture th;

    if (GenerateA(font, th, str, color, width, align, flags))
    {
        th.Draw(x, y);
    }
}

Wisp::CPoint2Di CFontsManager::GetCaretPosW(
    uint8_t font, const wstring &str, int pos, int width, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;

    Wisp::CPoint2Di p;
    if (pos < 1 || font >= 20 || (m_UnicodeFontAddress[font] == 0u) || str.empty())
    {
        return p;
    }

    if (width == 0)
    {
        width = GetWidthW(font, str);
    }

    PMULTILINES_FONT_INFO info =
        GetInfoW(font, str.c_str(), (int)str.length(), align, flags, width);
    if (info == nullptr)
    {
        return p;
    }

    uint32_t *table = (uint32_t *)m_UnicodeFontAddress[font];
    //loop throgh lines to get width and height
    while (info != nullptr)
    {
        p.X = 0;
        int len = info->CharCount;
        if (info->CharStart == pos)
        {
            // FIXME: check possible info leak here
            return p;
        }

        //if pos is not in this line, just skip this
        if (pos <= info->CharStart + len && (int)info->Data.size() >= len)
        {
            for (int i = 0; i < len; i++)
            {
                //collect data about width of each character
                const wchar_t &ch = info->Data[i].item;
                uint32_t offset = table[ch];
                if ((offset != 0u) && offset != 0xFFFFFFFF)
                {
                    uint8_t *cptr = (uint8_t *)((size_t)table + offset);
                    p.X += ((char)cptr[0] + (char)cptr[2] + 1);
                }
                else if (ch == L' ')
                {
                    p.X += UNICODE_SPACE_WIDTH;
                }

                if (info->CharStart + i + 1 == pos)
                {
                    return p;
                }
            }
        }

        //add to height if there's another line
        if (info->m_Next != nullptr)
        {
            p.Y += info->MaxHeight;
        }

        PMULTILINES_FONT_INFO ptr = info;
        info = info->m_Next;
        ptr->Data.clear();
        delete ptr;
    }

    return p;
}

int CFontsManager::CalculateCaretPosW(
    uint8_t font,
    const wstring &str,
    int x,
    int y,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    if (x < 0 || y < 0 || font >= 20 || (m_UnicodeFontAddress[font] == 0u) || str.empty())
    {
        return 0;
    }

    if (width == 0)
    {
        width = GetWidthW(font, str);
    }

    if (x >= width)
    {
        return (int)str.length();
    }

    PMULTILINES_FONT_INFO info =
        GetInfoW(font, str.c_str(), (int)str.length(), align, flags, width);
    if (info == nullptr)
    {
        return 0;
    }

    int height = 0;

    uint32_t *table = (uint32_t *)m_UnicodeFontAddress[font];
    int pos = 0;
    bool found = false;

    while (info != nullptr)
    {
        height += info->MaxHeight;
        width = 0;

        if (!found)
        {
            if (y < height)
            {
                int len = info->CharCount;

                for (int i = 0; i < len; i++)
                {
                    const wchar_t &ch = info->Data[i].item;
                    int offset = table[ch];

                    if ((offset != 0) && offset != 0xFFFFFFFF)
                    {
                        uint8_t *cptr = (uint8_t *)((size_t)table + offset);
                        width += ((char)cptr[0] + (char)cptr[2] + 1);
                    }
                    else if (ch == L' ')
                    {
                        width += UNICODE_SPACE_WIDTH;
                    }

                    if (width > x)
                    {
                        break;
                    }

                    pos++;
                }

                found = true;
            }
            else
            {
                pos += info->CharCount;
                pos++;
            }
        }

        PMULTILINES_FONT_INFO ptr = info;

        info = info->m_Next;

        ptr->Data.clear();
        delete ptr;
    }

    if (pos > (int)str.size())
    {
        pos = checked_cast<int>(str.size());
    }

    return pos;
}

int CFontsManager::GetWidthW(uint8_t font, const wstring &str)
{
    DEBUG_TRACE_FUNCTION;
    if (font >= 20 || (m_UnicodeFontAddress[font] == 0u) || str.empty())
    {
        return 0;
    }

    auto table = (uint32_t *)m_UnicodeFontAddress[font];
    int textLength = 0;
    int maxTextLength = 0;
    for (const auto &c : str)
    {
        uint32_t &offset = table[c];
        if ((offset != 0u) && offset != 0xFFFFFFFF)
        {
            uint8_t *ptr = (uint8_t *)((size_t)table + offset);
            textLength += ((char)ptr[0] + (char)ptr[2] + 1);
        }
        else if (c == L' ')
        {
            textLength += UNICODE_SPACE_WIDTH;
        }
        else if (c == L'\n' || c == L'\r')
        {
            maxTextLength = std::max(maxTextLength, textLength);
            textLength = 0;
        }
    }

    return std::max(maxTextLength, textLength);
}

int CFontsManager::GetWidthExW(
    uint8_t font, const wstring &str, int maxWidth, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    if (font >= 20 || (m_UnicodeFontAddress[font] == 0u) || str.empty())
    {
        return 0;
    }

    PMULTILINES_FONT_INFO info =
        GetInfoW(font, str.c_str(), (int)str.length(), align, flags, maxWidth);

    int textWidth = 0;
    while (info != nullptr)
    {
        if (info->Width > textWidth)
        {
            textWidth = info->Width;
        }

        PMULTILINES_FONT_INFO ptr = info;
        info = info->m_Next;
        ptr->Data.clear();
        delete ptr;
    }

    return textWidth;
}

int CFontsManager::GetHeightW(
    uint8_t font, const wstring &str, int width, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    if (font >= 20 || (m_UnicodeFontAddress[font] == 0u) || str.empty())
    {
        return 0;
    }

    if (width == 0)
    {
        width = GetWidthW(font, str);
    }

    PMULTILINES_FONT_INFO info =
        GetInfoW(font, str.c_str(), (int)str.length(), align, flags, width);

    int textHeight = 0;

    while (info != nullptr)
    {
        if (m_UseHTML)
        {
            textHeight += MAX_HTML_TEXT_HEIGHT;
        }
        else
        {
            textHeight += info->MaxHeight;
        }

        PMULTILINES_FONT_INFO ptr = info;

        info = info->m_Next;

        ptr->Data.clear();
        delete ptr;
    }

    return textHeight;
}

int CFontsManager::GetHeightW(PMULTILINES_FONT_INFO info)
{
    DEBUG_TRACE_FUNCTION;

    int textHeight = 0;

    for (; info != nullptr; info = info->m_Next)
    {
        if (m_UseHTML)
        {
            textHeight += MAX_HTML_TEXT_HEIGHT;
        }
        else
        {
            textHeight += info->MaxHeight;
        }
    }

    return textHeight;
}

wstring CFontsManager::GetTextByWidthW(uint8_t font, const wstring &str, int width, bool isCropped)
{
    DEBUG_TRACE_FUNCTION;
    if (font >= 20 || (m_UnicodeFontAddress[font] == 0u) || str.empty())
    {
        return wstring({});
    }

    uint32_t *table = (uint32_t *)m_UnicodeFontAddress[font];

    if (isCropped)
    {
        uint32_t offset = table[L'.'];

        if ((offset != 0u) && offset != 0xFFFFFFFF)
        {
            width -= ((*((uint8_t *)((size_t)table + offset + 2)) * 3) + 3);
        }
    }

    int textLength = 0;
    wstring result = {};

    for (const wchar_t &c : str)
    {
        uint32_t offset = table[c];
        char charWidth = 0;

        if ((offset != 0u) && offset != 0xFFFFFFFF)
        {
            uint8_t *ptr = (uint8_t *)((size_t)table + offset);
            charWidth = ((char)ptr[0] + (char)ptr[2] + 1);
        }
        else if (c == L' ')
        {
            charWidth = UNICODE_SPACE_WIDTH;
        }

        if (charWidth != 0)
        {
            textLength += charWidth;

            if (textLength > width)
            {
                break;
            }

            result += c;
        }
    }

    if (isCropped)
    {
        result += L"...";
    }

    return result;
}

uint16_t CFontsManager::GetWebLinkID(const wstring &link, uint32_t &color)
{
    DEBUG_TRACE_FUNCTION;
    return GetWebLinkID(ToString(link), color);
}

uint16_t CFontsManager::GetWebLinkID(const string &link, uint32_t &color)
{
    DEBUG_TRACE_FUNCTION;
    uint16_t linkID = 0;

    WEBLINK_MAP::iterator it = m_WebLink.begin();

    for (; it != m_WebLink.end(); ++it)
    {
        if ((*it).second.WebLink == link)
        {
            break;
        }
    }

    if (it == m_WebLink.end())
    {
        linkID = (uint16_t)m_WebLink.size() + 1;
        WEB_LINK wl = { false, link };
        m_WebLink[linkID] = wl;
    }
    else
    {
        if ((*it).second.Visited)
        {
            color = m_VisitedWebLinkColor;
        }

        linkID = (*it).first;
    }

    return linkID;
}

HTMLCHAR_LIST CFontsManager::GetHTMLData(
    uint8_t font, const wchar_t *str, int &len, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    HTMLCHAR_LIST data;

    if (len < 1)
    {
        return data;
    }

    data.resize(len);
    int newlen = 0;

    HTML_DATA_INFO info = { HTT_NONE, align, flags, font, m_HTMLColor, 0 };

    vector<HTML_DATA_INFO> stack;
    stack.push_back(info);

    HTML_DATA_INFO currentInfo = info;

    for (int i = 0; i < len; i++)
    {
        wchar_t si = str[i];

        if (si == L'<')
        {
            bool endTag = false;
            HTML_DATA_INFO newInfo = { HTT_NONE, TS_LEFT, 0, 0xFF, 0, 0 };

            HTML_TAG_TYPE tag = ParseHTMLTag(str, len, i, endTag, newInfo);

            if (tag == HTT_NONE)
            {
                continue;
            }

            if (!endTag)
            {
                if (newInfo.Font == 0xFF)
                {
                    newInfo.Font = stack.back().Font;
                }

                if (tag != HTT_BODY)
                {
                    stack.push_back(newInfo);
                }
                else
                {
                    stack.clear();
                    newlen = 0;

                    if (newInfo.Color != 0u)
                    {
                        info.Color = newInfo.Color;
                    }

                    stack.push_back(info);
                }
            }
            else if (stack.size() > 1)
            {
                //stack.pop_back();

                int index = -1;

                for (int j = (int)stack.size() - 1; j >= 1; j--)
                {
                    if (stack[j].Tag == tag)
                    {
                        stack.erase(stack.begin() + j);
                        break;
                    }
                }
            }

            currentInfo = GetCurrentHTMLInfo(stack);

            switch (tag)
            {
                case HTT_LEFT:
                case HTT_CENTER:
                case HTT_RIGHT:
                {
                    if (newlen != 0)
                    {
                        endTag = true;
                    }
                }
                case HTT_P:
                {
                    if (endTag)
                    {
                        si = L'\n';
                    }
                    else
                    {
                        si = 0;
                    }

                    break;
                }
                case HTT_BR:
                case HTT_BQ:
                {
                    si = L'\n';

                    break;
                }
                default:
                {
                    si = 0;

                    break;
                }
            }
        }

        if (si != 0)
        {
            data[newlen].Char = si;
            data[newlen].Font = currentInfo.Font;
            data[newlen].Align = currentInfo.Align;
            data[newlen].Flags = currentInfo.Flags;
            data[newlen].Color = currentInfo.Color;
            data[newlen].LinkID = currentInfo.Link;

            newlen++;
        }
    }

    data.resize(newlen);
    len = newlen;

    return data;
}

HTML_DATA_INFO CFontsManager::GetHTMLInfoFromTag(const HTML_TAG_TYPE &tag)
{
    DEBUG_TRACE_FUNCTION;
    HTML_DATA_INFO info = { tag, TS_LEFT, 0, 0xFF, 0, 0 };

    switch (tag)
    {
        case HTT_B:
            info.Flags = UOFONT_SOLID;
            break;
        case HTT_I:
            info.Flags = UOFONT_ITALIC;
            break;
        case HTT_U:
            info.Flags = UOFONT_UNDERLINE;
            break;
        case HTT_P:
            info.Flags = UOFONT_INDENTION;
            break;
        case HTT_BIG:
            info.Font = 0;
            break;
        case HTT_SMALL:
            info.Font = 2;
            break;
        case HTT_H1:
            info.Flags = UOFONT_SOLID | UOFONT_UNDERLINE;
            info.Font = 0;
            break;
        case HTT_H2:
            info.Flags = UOFONT_SOLID;
            info.Font = 0;
            break;
        case HTT_H3:
            info.Font = 0;
            break;
        case HTT_H4:
            info.Flags = UOFONT_SOLID;
            info.Font = 2;
            break;
        case HTT_H5:
            info.Flags = UOFONT_ITALIC;
            info.Font = 2;
            break;
        case HTT_H6:
            info.Font = 2;
            break;
        case HTT_BQ:
            info.Flags = UOFONT_BQ;
            info.Color = 0x008000FF;
            break;
        case HTT_LEFT:
            info.Align = TS_LEFT;
            break;
        case HTT_CENTER:
            info.Align = TS_CENTER;
            break;
        case HTT_RIGHT:
            info.Align = TS_RIGHT;
            break;
        default:
            break;
    }

    return info;
}

HTML_DATA_INFO CFontsManager::GetCurrentHTMLInfo(const HTMLINFO_LIST &list)
{
    DEBUG_TRACE_FUNCTION;
    HTML_DATA_INFO info = { HTT_NONE, TS_LEFT, 0, 0xFF, 0, 0 };

    for (int i = 0; i < (int)list.size(); i++)
    {
        const HTML_DATA_INFO &current = list[i];

        switch (current.Tag)
        {
            case HTT_NONE:
                info = current;
                break;
            case HTT_B:
            case HTT_I:
            case HTT_U:
            case HTT_P:
                info.Flags |= current.Flags;
                break;
            case HTT_A:
            {
                info.Flags |= current.Flags;
                info.Color = current.Color;
                info.Link = current.Link;

                break;
            }
            case HTT_BIG:
            case HTT_SMALL:
                if (current.Font != 0xFF && (m_UnicodeFontAddress[current.Font] != 0u))
                {
                    info.Font = current.Font;
                }
                break;
            case HTT_BASEFONT:
            {
                if (current.Font != 0xFF && (m_UnicodeFontAddress[current.Font] != 0u))
                {
                    info.Font = current.Font;
                }

                if (current.Color != 0)
                {
                    info.Color = current.Color;
                }

                break;
            }
            case HTT_H1:
            case HTT_H2:
            case HTT_H4:
            case HTT_H5:
                info.Flags |= current.Flags;
            case HTT_H3:
            case HTT_H6:
                if (current.Font != 0xFF && (m_UnicodeFontAddress[current.Font] != 0u))
                {
                    info.Font = current.Font;
                }
                break;
            case HTT_BQ:
                info.Color = current.Color;
                info.Flags |= current.Flags;
                break;
            case HTT_LEFT:
            case HTT_CENTER:
            case HTT_RIGHT:
                info.Align = current.Align;
                break;
            case HTT_DIV:
                info.Align = current.Align;
                break;
            default:
                break;
        }
    }

    return info;
}

void CFontsManager::TrimHTMLString(string &str)
{
    DEBUG_TRACE_FUNCTION;
    if (str.length() >= 2 && str[0] == '"' && str[str.length() - 1] == '"')
    {
        str.resize(str.length() - 1);
        str.erase(str.begin());
    }
}

uint32_t CFontsManager::GetHTMLColorFromText(string &str)
{
    DEBUG_TRACE_FUNCTION;
    uint32_t color = 0;

    if (str.length() > 1)
    {
        if (str[0] == '#')
        {
            char *end;
            color = strtoul(str.c_str() + 1, &end, 16);

            uint8_t *clrBuf = (uint8_t *)&color;
            color = (clrBuf[0] << 24) | (clrBuf[1] << 16) | (clrBuf[2] << 8) | 0xFF;
        }
        else
        {
            str = ToLowerA(str);

            if (str == "red")
            {
                color = 0x0000FFFF;
            }
            else if (str == "cyan")
            {
                color = 0xFFFF00FF;
            }
            else if (str == "blue")
            {
                color = 0xFF0000FF;
            }
            else if (str == "darkblue")
            {
                color = 0xA00000FF;
            }
            else if (str == "lightblue")
            {
                color = 0xE6D8ADFF;
            }
            else if (str == "purple")
            {
                color = 0x800080FF;
            }
            else if (str == "yellow")
            {
                color = 0x00FFFFFF;
            }
            else if (str == "lime")
            {
                color = 0x00FF00FF;
            }
            else if (str == "magenta")
            {
                color = 0xFF00FFFF;
            }
            else if (str == "white")
            {
                color = 0xFFFEFEFF;
            }
            else if (str == "silver")
            {
                color = 0xC0C0C0FF;
            }
            else if (str == "gray" || str == "grey")
            {
                color = 0x808080FF;
            }
            else if (str == "black")
            {
                color = 0x010101FF;
            }
            else if (str == "orange")
            {
                color = 0x00A5FFFF;
            }
            else if (str == "brown")
            {
                color = 0x2A2AA5FF;
            }
            else if (str == "maroon")
            {
                color = 0x000080FF;
            }
            else if (str == "green")
            {
                color = 0x008000FF;
            }
            else if (str == "olive")
            {
                color = 0x008080FF;
            }
        }
    }

    return color;
}

void CFontsManager::GetHTMLInfoFromContent(HTML_DATA_INFO &info, const string &content)
{
    DEBUG_TRACE_FUNCTION;
    Wisp::CTextFileParser parser({}, " =", "", "\"\"");

    vector<string> strings = parser.GetTokens(content.c_str());
    int size = (int)strings.size();

    for (int i = 0; i < size; i += 2)
    {
        if (i + 1 >= size)
        {
            break;
        }

        string str = ToLowerA(strings[i]);
        string &value = strings[i + 1];
        TrimHTMLString(value);

        if (value.length() == 0u)
        {
            continue;
        }

        switch (info.Tag)
        {
            case HTT_BODY:
            {
                if (str == "text")
                {
                    info.Color = GetHTMLColorFromText(value);
                }
                else if (m_HTMLBackgroundCanBeColored && str == "bgcolor")
                {
                    m_BackgroundColor = GetHTMLColorFromText(value);
                }
                else if (str == "link")
                {
                    m_WebLinkColor = GetHTMLColorFromText(value);
                }
                else if (str == "vlink")
                {
                    m_VisitedWebLinkColor = GetHTMLColorFromText(value);
                }
                else if (str == "leftmargin")
                {
                    m_LeftMargin = atoi(value.c_str());
                }
                else if (str == "topmargin")
                {
                    m_TopMargin = atoi(value.c_str());
                }
                else if (str == "rightmargin")
                {
                    m_RightMargin = atoi(value.c_str());
                }
                else if (str == "bottommargin")
                {
                    m_BottomMargin = atoi(value.c_str());
                }

                break;
            }
            case HTT_BASEFONT:
            {
                if (str == "color")
                {
                    info.Color = GetHTMLColorFromText(value);
                }
                else if (str == "size")
                {
                    uint8_t font = atoi(value.c_str());

                    if ((font == 0) || (font == 4))
                    {
                        info.Font = 1;
                    }
                    else if (font < 4)
                    {
                        info.Font = 2;
                    }
                    else
                    {
                        info.Font = 0;
                    }
                }

                break;
            }
            case HTT_A:
            {
                if (str == "href")
                {
                    info.Flags = UOFONT_UNDERLINE;
                    info.Color = m_WebLinkColor;
                    info.Link = GetWebLinkID(value, info.Color);
                }

                break;
            }
            case HTT_DIV:
            {
                if (str == "align")
                {
                    str = ToLowerA(value);

                    if (str == "left")
                    {
                        info.Align = TS_LEFT;
                    }
                    else if (str == "center")
                    {
                        info.Align = TS_CENTER;
                    }
                    else if (str == "right")
                    {
                        info.Align = TS_RIGHT;
                    }
                }

                break;
            }
            default:
                break;
        }
    }
}

HTML_TAG_TYPE
CFontsManager::ParseHTMLTag(const wchar_t *str, int len, int &i, bool &endTag, HTML_DATA_INFO &info)
{
    DEBUG_TRACE_FUNCTION;
    HTML_TAG_TYPE tag = HTT_NONE;

    i++;

    if (i < len && str[i] == L'/')
    {
        endTag = true;

        i++;
    }

    while (str[i] == L' ' && i < len)
    {
        i++;
    }

    int j = i;

    for (; i < len; i++)
    {
        if (str[i] == L' ' || str[i] == L'>')
        {
            break;
        }
    }

    if (j != i && i < len)
    {
        int cmdLen = i - j;
        //LOG("cmdLen = %i\n", cmdLen);
        wstring cmd;
        cmd.resize(cmdLen);
        memcpy(&cmd[0], &str[j], cmdLen * 2);
        //LOG(L"cmd[%s] = %s\n", (endTag ? L"end" : L"start"), cmd.c_str());
        cmd = ToLowerW(cmd);

        j = i;

        while (str[i] != L'>' && i < len)
        {
            i++;
        }

        if (cmd == L"b")
        {
            tag = HTT_B;
        }
        else if (cmd == L"i")
        {
            tag = HTT_I;
        }
        else if (cmd == L"a")
        {
            tag = HTT_A;
        }
        else if (cmd == L"u")
        {
            tag = HTT_U;
        }
        else if (cmd == L"p")
        {
            tag = HTT_P;
        }
        else if (cmd == L"big")
        {
            tag = HTT_BIG;
        }
        else if (cmd == L"small")
        {
            tag = HTT_SMALL;
        }
        else if (cmd == L"body")
        {
            tag = HTT_BODY;
        }
        else if (cmd == L"basefont")
        {
            tag = HTT_BASEFONT;
        }
        else if (cmd == L"h1")
        {
            tag = HTT_H1;
        }
        else if (cmd == L"h2")
        {
            tag = HTT_H2;
        }
        else if (cmd == L"h3")
        {
            tag = HTT_H3;
        }
        else if (cmd == L"h4")
        {
            tag = HTT_H4;
        }
        else if (cmd == L"h5")
        {
            tag = HTT_H5;
        }
        else if (cmd == L"h6")
        {
            tag = HTT_H6;
        }
        else if (cmd == L"br")
        {
            tag = HTT_BR;
        }
        else if (cmd == L"bq")
        {
            tag = HTT_BQ;
        }
        else if (cmd == L"left")
        {
            tag = HTT_LEFT;
        }
        else if (cmd == L"center")
        {
            tag = HTT_CENTER;
        }
        else if (cmd == L"right")
        {
            tag = HTT_RIGHT;
        }
        else if (cmd == L"div")
        {
            tag = HTT_DIV;
        }

        if (!endTag)
        {
            info = GetHTMLInfoFromTag(tag);

            if (i < len && j != i)
            {
                switch (tag)
                {
                    case HTT_BODY:
                    case HTT_BASEFONT:
                    case HTT_A:
                    case HTT_DIV:
                    {
                        wstring content = {};
                        cmdLen = i - j;
                        //LOG("contentCmdLen = %i\n", cmdLen);
                        content.resize(cmdLen);
                        memcpy(&content[0], &str[j], cmdLen * 2);
                        //LOG(L"contentCmd = %s\n", content.c_str());

                        if (static_cast<unsigned int>(!content.empty()) != 0u)
                        {
                            GetHTMLInfoFromContent(info, ToString(content));
                        }

                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }

    return tag;
}

PMULTILINES_FONT_INFO CFontsManager::GetInfoHTML(
    uint8_t font, const wchar_t *str, int len, TEXT_ALIGN_TYPE align, uint16_t flags, int width)
{
    DEBUG_TRACE_FUNCTION;
    HTMLCHAR_LIST htmlData = GetHTMLData(font, str, len, align, flags);

    if (htmlData.empty())
    {
        return nullptr;
    }

    PMULTILINES_FONT_INFO info = new MULTILINES_FONT_INFO();
    info->Reset();
    info->Align = align;

    PMULTILINES_FONT_INFO ptr = info;

    int indentionOffset = 0;

    ptr->IndentionOffset = indentionOffset;

    int charCount = 0;
    int lastSpace = 0;
    int readWidth = 0;

    bool isFixed = ((flags & UOFONT_FIXED) != 0);
    bool isCropped = ((flags & UOFONT_CROPPED) != 0);

    if (len != 0)
    {
        ptr->Align = htmlData[0].Align;
    }

    for (int i = 0; i < len; i++)
    {
        wchar_t si = htmlData[i].Char;
        uint32_t *table = (uint32_t *)m_UnicodeFontAddress[htmlData[i].Font];

        if (si == 0x000D || si == L'\n')
        {
            if (si == 0x000D || isFixed || isCropped)
            {
                si = 0;
            }
            else
            {
                si = L'\n';
            }
        }

        if (((table[si] == 0u) || table[si] == 0xFFFFFFFF) && si != L' ' && si != L'\n')
        {
            continue;
        }

        uint8_t *data = (uint8_t *)((size_t)table + table[si]);

        if (si == L' ')
        {
            lastSpace = i;
            ptr->Width += readWidth;
            readWidth = 0;
            ptr->CharCount += charCount;
            charCount = 0;
        }

        int solidWidth = (int)(htmlData[i].Flags & UOFONT_SOLID);

        if (ptr->Width + readWidth + ((char)data[0] + (char)data[2]) + solidWidth > width ||
            si == L'\n')
        {
            if (lastSpace == ptr->CharStart && (lastSpace == 0) && si != L'\n')
            {
                ptr->CharStart = 1;
            }

            if (si == L'\n')
            {
                ptr->Width += readWidth;
                ptr->CharCount += charCount;

                lastSpace = i;

                if (ptr->Width == 0)
                {
                    ptr->Width = 1;
                }

                // (!ptr->MaxHeight)
                //ptr->MaxHeight = 14;
                ptr->MaxHeight = MAX_HTML_TEXT_HEIGHT;

                ptr->Data.resize(ptr->CharCount);

                PMULTILINES_FONT_INFO newptr = new MULTILINES_FONT_INFO();
                newptr->Reset();

                ptr->m_Next = newptr;

                ptr = newptr;

                ptr->Align = htmlData[i].Align;
                ptr->CharStart = i + 1;

                readWidth = 0;
                charCount = 0;
                indentionOffset = 0;

                ptr->IndentionOffset = indentionOffset;

                continue;
            }
            if (lastSpace + 1 == ptr->CharStart && !isFixed && !isCropped)
            {
                ptr->Width += readWidth;
                ptr->CharCount += charCount;

                if (ptr->Width == 0)
                {
                    ptr->Width = 1;
                }

                //if (!ptr->MaxHeight)
                //ptr->MaxHeight = 10;
                ptr->MaxHeight = MAX_HTML_TEXT_HEIGHT;

                PMULTILINES_FONT_INFO newptr = new MULTILINES_FONT_INFO();
                newptr->Reset();

                ptr->m_Next = newptr;

                ptr = newptr;

                ptr->Align = htmlData[i].Align;
                ptr->CharStart = i;
                lastSpace = i - 1;
                charCount = 0;

                if (ptr->Align == TS_LEFT && ((htmlData[i].Flags & UOFONT_INDENTION) != 0))
                {
                    indentionOffset = 14;
                }

                ptr->IndentionOffset = indentionOffset;

                readWidth = (int)indentionOffset;
            }
            else
            {
                if (isFixed)
                {
                    MULTILINES_FONT_DATA mfd = { si,
                                                 htmlData[i].Flags,
                                                 htmlData[i].Font,
                                                 htmlData[i].LinkID,
                                                 htmlData[i].Color,
                                                 nullptr };
                    ptr->Data.push_back(mfd);

                    readWidth += ((char)data[0] + (char)data[2] + 1);

                    //if (((char)data[1] + (char)data[3]) > ptr->MaxHeight)
                    //ptr->MaxHeight = ((char)data[1] + (char)data[3]);
                    ptr->MaxHeight = MAX_HTML_TEXT_HEIGHT;

                    charCount++;

                    ptr->Width += readWidth;
                    ptr->CharCount += charCount;
                }

                i = lastSpace + 1;

                si = htmlData[i].Char;
                solidWidth = (int)(htmlData[i].Flags & UOFONT_SOLID);

                if (ptr->Width == 0)
                {
                    ptr->Width = 1;
                }

                //if (!ptr->MaxHeight)
                //ptr->MaxHeight = 10;
                ptr->MaxHeight = MAX_HTML_TEXT_HEIGHT;

                ptr->Data.resize(ptr->CharCount);
                charCount = 0;

                if (isFixed || isCropped)
                {
                    break;
                }

                PMULTILINES_FONT_INFO newptr = new MULTILINES_FONT_INFO();
                newptr->Reset();

                ptr->m_Next = newptr;

                ptr = newptr;

                ptr->Align = htmlData[i].Align;
                ptr->CharStart = i;

                if (ptr->Align == TS_LEFT && ((htmlData[i].Flags & UOFONT_INDENTION) != 0))
                {
                    indentionOffset = 14;
                }

                ptr->IndentionOffset = indentionOffset;

                readWidth = indentionOffset;
            }
        }

        MULTILINES_FONT_DATA mfd = {
            si, htmlData[i].Flags, htmlData[i].Font, htmlData[i].LinkID, htmlData[i].Color, nullptr
        };
        ptr->Data.push_back(mfd);

        if (si == L' ')
        {
            readWidth += UNICODE_SPACE_WIDTH;

            //if (!ptr->MaxHeight)
            //ptr->MaxHeight = 5;
        }
        else
        {
            readWidth += ((char)data[0] + (char)data[2] + 1) + solidWidth;

            //if (((char)data[1] + (char)data[3]) > ptr->MaxHeight)
            //->MaxHeight = ((char)data[1] + (char)data[3]);
        }

        charCount++;
    }

    ptr->Width += readWidth;
    ptr->CharCount += charCount;
    ptr->MaxHeight = MAX_HTML_TEXT_HEIGHT;

    return info;
}

PMULTILINES_FONT_INFO CFontsManager::GetInfoW(
    uint8_t font, const wchar_t *str, int len, TEXT_ALIGN_TYPE align, uint16_t flags, int width)
{
    DEBUG_TRACE_FUNCTION;
    m_WebLinkColor = 0xFF0000FF;
    m_VisitedWebLinkColor = 0x0000FFFF;
    m_BackgroundColor = 0;
    m_LeftMargin = 0;
    m_TopMargin = 0;
    m_RightMargin = 0;
    m_BottomMargin = 0;

    if (font >= 20 || (m_UnicodeFontAddress[font] == 0u))
    {
        return nullptr;
    }

    if (m_UseHTML)
    {
        return GetInfoHTML(font, str, len, align, flags, width);
    }

    uint32_t *table = (uint32_t *)m_UnicodeFontAddress[font];

    PMULTILINES_FONT_INFO info = new MULTILINES_FONT_INFO();
    info->Reset();
    info->Align = align;

    PMULTILINES_FONT_INFO ptr = info;

    int indentionOffset = 0;

    ptr->IndentionOffset = 0;

    int charCount = 0;
    int lastSpace = 0;
    int readWidth = 0;

    bool isFixed = ((flags & UOFONT_FIXED) != 0);
    bool isCropped = ((flags & UOFONT_CROPPED) != 0);

    TEXT_ALIGN_TYPE current_align = align;
    uint16_t current_flags = flags;
    uint8_t current_font = font;
    uint32_t charcolor = 0xFFFFFFFF;
    uint32_t current_charcolor = 0xFFFFFFFF;
    uint32_t lastspace_charcolor = 0xFFFFFFFF;
    uint32_t lastspace_current_charcolor = 0xFFFFFFFF;

    for (int i = 0; i < len; i++)
    {
        wchar_t si = str[i];

        if (si == L'\r' || si == L'\n')
        {
            if (isFixed || isCropped)
            {
                si = 0;
            }
            else
            {
                si = L'\n';
            }
        }

        if (((table[si] == 0u) || table[si] == 0xFFFFFFFF) && si != L' ' && si != L'\n')
        {
            continue;
        }

        uint8_t *data = (uint8_t *)((size_t)table + table[si]);

        if (si == L' ')
        {
            lastSpace = i;
            ptr->Width += readWidth;
            readWidth = 0;
            ptr->CharCount += charCount;
            charCount = 0;
            lastspace_charcolor = charcolor;
            lastspace_current_charcolor = current_charcolor;
        }

        if (ptr->Width + readWidth + ((char)data[0] + (char)data[2]) > width || si == L'\n')
        {
            if (lastSpace == ptr->CharStart && (lastSpace == 0) && si != L'\n')
            {
                ptr->CharStart = 1;
            }

            if (si == L'\n')
            {
                ptr->Width += readWidth;
                ptr->CharCount += charCount;

                lastSpace = i;

                if (ptr->Width == 0)
                {
                    ptr->Width = 1;
                }

                if (ptr->MaxHeight == 0)
                {
                    ptr->MaxHeight = 14;
                }

                ptr->Data.resize(ptr->CharCount);

                PMULTILINES_FONT_INFO newptr = new MULTILINES_FONT_INFO();
                newptr->Reset();

                ptr->m_Next = newptr;

                ptr = newptr;

                ptr->Align = current_align;
                ptr->CharStart = i + 1;

                readWidth = 0;
                charCount = 0;
                indentionOffset = 0;

                ptr->IndentionOffset = 0;

                continue;
            }
            if (lastSpace + 1 == ptr->CharStart && !isFixed && !isCropped)
            {
                ptr->Width += readWidth;
                ptr->CharCount += charCount;

                if (ptr->Width == 0)
                {
                    ptr->Width = 1;
                }

                if (ptr->MaxHeight == 0)
                {
                    ptr->MaxHeight = 14;
                }

                PMULTILINES_FONT_INFO newptr = new MULTILINES_FONT_INFO();
                newptr->Reset();

                ptr->m_Next = newptr;

                ptr = newptr;

                ptr->Align = current_align;
                ptr->CharStart = i;
                lastSpace = i - 1;
                charCount = 0;

                if (ptr->Align == TS_LEFT && ((current_flags & UOFONT_INDENTION) != 0))
                {
                    indentionOffset = 14;
                }

                ptr->IndentionOffset = indentionOffset;

                readWidth = indentionOffset;
            }
            else
            {
                if (isFixed)
                {
                    MULTILINES_FONT_DATA mfd = { si, current_flags,     current_font,
                                                 0,  current_charcolor, nullptr };
                    ptr->Data.push_back(mfd);

                    readWidth += ((char)data[0] + (char)data[2] + 1);

                    if (((char)data[1] + (char)data[3]) > ptr->MaxHeight)
                    {
                        ptr->MaxHeight = ((char)data[1] + (char)data[3]);
                    }

                    charCount++;

                    ptr->Width += readWidth;
                    ptr->CharCount += charCount;
                }

                i = lastSpace + 1;

                charcolor = lastspace_charcolor;
                current_charcolor = lastspace_current_charcolor;
                si = str[i];

                if (ptr->Width == 0)
                {
                    ptr->Width = 1;
                }

                if (ptr->MaxHeight == 0)
                {
                    ptr->MaxHeight = 14;
                }

                ptr->Data.resize(ptr->CharCount);

                if (isFixed || isCropped)
                {
                    break;
                }

                PMULTILINES_FONT_INFO newptr = new MULTILINES_FONT_INFO();
                newptr->Reset();

                ptr->m_Next = newptr;

                ptr = newptr;

                ptr->Align = current_align;
                ptr->CharStart = i;
                charCount = 0;

                if (ptr->Align == TS_LEFT && ((current_flags & UOFONT_INDENTION) != 0))
                {
                    indentionOffset = 14;
                }

                ptr->IndentionOffset = indentionOffset;

                readWidth = indentionOffset;
            }
        }

        MULTILINES_FONT_DATA mfd = {
            si, current_flags, current_font, 0, current_charcolor, nullptr
        };
        ptr->Data.push_back(mfd);

        if (si == L' ')
        {
            readWidth += UNICODE_SPACE_WIDTH;

            if (ptr->MaxHeight == 0)
            {
                ptr->MaxHeight = 5;
            }
        }
        else
        {
            readWidth += ((char)data[0] + (char)data[2] + 1);

            if (((char)data[1] + (char)data[3]) > ptr->MaxHeight)
            {
                ptr->MaxHeight = ((char)data[1] + (char)data[3]);
            }
        }

        charCount++;
    }

    ptr->Width += readWidth;
    ptr->CharCount += charCount;

    if ((readWidth == 0) && (len != 0) && (str[len - 1] == L'\n' || str[len - 1] == L'\r'))
    {
        ptr->Width = 1;
        ptr->MaxHeight = 14;
    }

    return info;
}

bool CFontsManager::GenerateW(
    uint8_t font,
    CGLTextTexture &th,
    const wstring &str,
    uint16_t color,
    uint8_t cell,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    if (((flags & UOFONT_FIXED) != 0) || ((flags & UOFONT_CROPPED) != 0))
    {
        th.Clear();

        if ((width == 0) || str.empty())
        {
            return false;
        }

        int realWidth = GetWidthW(font, str);

        if (realWidth > width)
        {
            wstring newstr = GetTextByWidthW(font, str, width, (flags & UOFONT_CROPPED) != 0);
            return GenerateWBase(font, th, newstr, color, cell, width, align, flags);
        }
    }

    return GenerateWBase(font, th, str, color, cell, width, align, flags);
}

vector<uint32_t> CFontsManager::GeneratePixelsW(
    uint8_t font,
    CGLTextTexture &th,
    const wchar_t *str,
    uint16_t color,
    uint8_t cell,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    vector<uint32_t> pData;

    if (font >= 20 || (m_UnicodeFontAddress[font] == 0u))
    {
        return pData;
    }

    int len = lstrlenW(str);

    if (len == 0)
    {
        return pData;
    }

    int oldWidth = width;

    if (width == 0)
    {
        width = GetWidthW(font, str);

        if (width == 0)
        {
            return pData;
        }
    }

    PMULTILINES_FONT_INFO info = GetInfoW(font, str, len, align, flags, width);

    if (info == nullptr)
    {
        return pData;
    }

    if (m_UseHTML && ((m_LeftMargin != 0) || (m_RightMargin != 0)))
    {
        while (info != nullptr)
        {
            PMULTILINES_FONT_INFO ptr = info->m_Next;

            info->Data.clear();
            delete info;

            info = ptr;
        }

        int newWidth = width - (m_LeftMargin + m_RightMargin);

        if (newWidth < 10)
        {
            newWidth = 10;
        }

        info = GetInfoW(font, str, len, align, flags, newWidth);

        if (info == nullptr)
        {
            return pData;
        }
    }

    if ((oldWidth == 0) && RecalculateWidthByInfo)
    {
        PMULTILINES_FONT_INFO ptr = info;
        width = 0;

        while (ptr != nullptr)
        {
            if (ptr->Width > width)
            {
                width = ptr->Width;
            }

            ptr = ptr->m_Next;
        }
    }

    width += 4;

    int height = GetHeightW(info);

    if (height == 0)
    {
        while (info != nullptr)
        {
            PMULTILINES_FONT_INFO ptr = info;

            info = info->m_Next;

            ptr->Data.clear();
            delete ptr;
        }

        return pData;
    }

    height += m_TopMargin + m_BottomMargin + 4;

    int blocksize = height * width;

    pData.resize(blocksize, 0);

    uint32_t *table = (uint32_t *)m_UnicodeFontAddress[font];

    int lineOffsY = 1 + m_TopMargin;

    PMULTILINES_FONT_INFO ptr = info;

    uint32_t datacolor = 0;

    if (/*m_UseHTML &&*/ color == 0xFFFF)
    {
        datacolor = 0xFFFFFFFE;
    }
    else
    {
        datacolor = g_ColorManager.GetPolygoneColor(cell, color) << 8 | 0xFF;
    }

    bool isItalic = (flags & UOFONT_ITALIC) != 0;
    bool isSolid = (flags & UOFONT_SOLID) != 0;
    bool isBlackBorder = (flags & UOFONT_BLACK_BORDER) != 0;
    bool isUnderline = (flags & UOFONT_UNDERLINE) != 0;
    uint32_t blackColor = 0x010101FF;

    bool isLink = false;
    int linkStartX = 0;
    int linkStartY = 0;

    while (ptr != nullptr)
    {
        info = ptr;

        th.LinesCount = th.LinesCount + 1;

        int w = m_LeftMargin;

        if (ptr->Align == TS_CENTER)
        {
            w += (((width - 10) - ptr->Width) / 2);
            if (w < 0)
            {
                w = 0;
            }
        }
        else if (ptr->Align == TS_RIGHT)
        {
            w += ((width - 10) - ptr->Width);

            if (w < 0)
            {
                w = 0;
            }
        }
        else if (ptr->Align == TS_LEFT && ((flags & UOFONT_INDENTION) != 0))
        {
            w += ptr->IndentionOffset;
        }

        uint16_t oldLink = 0;

        int dataSize = (int)ptr->Data.size();
        for (int i = 0; i < dataSize; i++)
        {
            const MULTILINES_FONT_DATA &dataPtr = ptr->Data[i];
            const wchar_t &si = dataPtr.item;

            table = (uint32_t *)m_UnicodeFontAddress[dataPtr.font];

            if (!isLink)
            {
                oldLink = dataPtr.linkID;

                if (oldLink != 0u)
                {
                    isLink = true;
                    linkStartX = w;
                    linkStartY = lineOffsY + 3;
                }
            }
            else if ((dataPtr.linkID == 0u) || i + 1 == dataSize)
            {
                isLink = false;
                int linkHeight = lineOffsY - linkStartY;
                if (linkHeight < 14)
                {
                    linkHeight = 14;
                }

                int ofsX = 0;

                if (si == L' ')
                {
                    ofsX = UNICODE_SPACE_WIDTH;
                }
                else if (((table[si] == 0u) || table[si] == 0xFFFFFFFF) && si != L' ')
                {
                }
                else
                {
                    uint8_t *xData = (uint8_t *)((size_t)table + table[si]);
                    ofsX = (char)xData[2];
                }

                WEB_LINK_RECT wlr = { oldLink, linkStartX, linkStartY, w - ofsX, linkHeight };
                th.AddWebLink(wlr);
                oldLink = 0;
            }

            /*if (m_UseHTML)
			{
			if (i >= ptr->Data.size()) break;
			si = ptr->Data[i].item;
			}*/

            if (((table[si] == 0u) || table[si] == 0xFFFFFFFF) && si != L' ')
            {
                continue;
            }

            uint8_t *data = (uint8_t *)((size_t)table + table[si]);

            int offsX = 0;
            int offsY = 0;
            int dw = 0;
            int dh = 0;

            if (si == L' ')
            {
                offsX = 0;
                dw = UNICODE_SPACE_WIDTH;
            }
            else
            {
                offsX = (char)data[0] + 1;
                offsY = (char)data[1];
                dw = (char)data[2];
                dh = (char)data[3];

                data += 4;
            }

            int tmpW = w;
            uint32_t charcolor = datacolor;
            bool isBlackPixel =
                (((charcolor >> 24) & 0xFF) <= 8 && ((charcolor >> 16) & 0xFF) <= 8 &&
                 ((charcolor >> 8) & 0xFF) <= 8);

            if (si != L' ')
            {
                if (m_UseHTML && i < (int)ptr->Data.size())
                {
                    isItalic = ((dataPtr.flags & UOFONT_ITALIC) != 0);
                    isSolid = ((dataPtr.flags & UOFONT_SOLID) != 0);
                    isBlackBorder = ((dataPtr.flags & UOFONT_BLACK_BORDER) != 0);
                    isUnderline = ((dataPtr.flags & UOFONT_UNDERLINE) != 0);

                    if (dataPtr.color != 0xFFFFFFFF)
                    {
                        charcolor = dataPtr.color;
                        isBlackPixel =
                            (((charcolor >> 24) & 0xFF) <= 8 && ((charcolor >> 16) & 0xFF) <= 8 &&
                             ((charcolor >> 8) & 0xFF) <= 8);
                    }
                }

                int scanlineCount = (int)((dw - 1) / 8) + 1;

                for (int y = 0; y < dh; y++)
                {
                    int testY = offsY + lineOffsY + y;

                    if (testY >= height)
                    {
                        break;
                    }

                    uint8_t *scanlines = data;
                    data += scanlineCount;

                    int italicOffset = 0;
                    if (isItalic)
                    {
                        italicOffset = (int)((dh - y) / ITALIC_FONT_KOEFFICIENT);
                    }

                    int testX = w + offsX + italicOffset + (int)isSolid;

                    for (int c = 0; c < scanlineCount; c++)
                    {
                        for (int j = 0; j < 8; j++)
                        {
                            int x = ((int)c * 8) + j;

                            if (x >= dw)
                            {
                                break;
                            }

                            int nowX = testX + x;

                            if (nowX >= width)
                            {
                                break;
                            }

                            uint8_t cl = scanlines[c] & (1 << (7 - j));

                            int block = (testY * width) + nowX;

                            if (cl != 0u)
                            {
                                pData[block] = charcolor;
                            }
                        }
                    }
                }

                if (isSolid)
                {
                    uint32_t solidColor = blackColor;

                    if (solidColor == charcolor)
                    {
                        solidColor++;
                    }

                    int minXOk = ((w + offsX) > 0) ? -1 : 0;
                    int maxXOk = ((w + offsX + dw) < width) ? 1 : 0;

                    maxXOk += dw;

                    for (int cy = 0; cy < dh; cy++)
                    {
                        int testY = offsY + lineOffsY + cy;

                        if (testY >= height)
                        {
                            break;
                        }

                        int italicOffset = 0;
                        if (isItalic && cy < dh)
                        {
                            italicOffset = (int)((dh - cy) / ITALIC_FONT_KOEFFICIENT);
                        }

                        for (int cx = minXOk; cx < maxXOk; cx++)
                        {
                            int testX = cx + w + offsX + italicOffset;

                            if (testX >= width /* + italicOffset*/)
                            {
                                break;
                            }

                            int block = (testY * width) + testX;

                            if ((pData[block] == 0u) && pData[block] != solidColor)
                            {
                                int endX = (cx < dw) ? 2 : 1;

                                if (endX == 2 && (testX + 1) >= width)
                                {
                                    endX--;
                                }

                                for (int x = 0; x < endX; x++)
                                {
                                    int nowX = testX + x;

                                    int testBlock = (testY * width) + nowX;

                                    if ((pData[testBlock] != 0u) && pData[testBlock] != solidColor)
                                    {
                                        pData[block] = solidColor;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    for (int cy = 0; cy < dh; cy++)
                    {
                        int testY = offsY + lineOffsY + cy;

                        if (testY >= height)
                        {
                            break;
                        }

                        int italicOffset = 0;
                        if (isItalic)
                        {
                            italicOffset = (int)((dh - cy) / ITALIC_FONT_KOEFFICIENT);
                        }

                        for (int cx = 0; cx < dw; cx++)
                        {
                            int testX = cx + w + offsX + italicOffset;

                            if (testX >= width /* + italicOffset*/)
                            {
                                break;
                            }

                            int block = (testY * width) + testX;

                            if (pData[block] == solidColor)
                            {
                                pData[block] = charcolor;
                            }
                        }
                    }
                }

                if (isBlackBorder && !isBlackPixel)
                {
                    int minXOk = (w + offsX > 0) ? -1 : 0;
                    int minYOk = (offsY + lineOffsY > 0) ? -1 : 0;
                    int maxXOk = (w + offsX + dw < width) ? 1 : 0;
                    int maxYOk = (offsY + lineOffsY + dh < height) ? 1 : 0;

                    maxXOk += dw;
                    maxYOk += dh;

                    for (int cy = minYOk; cy < maxYOk; cy++)
                    {
                        int testY = offsY + lineOffsY + cy;

                        if (testY >= height)
                        {
                            break;
                        }

                        int italicOffset = 0;
                        if (isItalic && cy >= 0 && cy < dh)
                        {
                            italicOffset = (int)((dh - cy) / ITALIC_FONT_KOEFFICIENT);
                        }

                        for (int cx = minXOk; cx < maxXOk; cx++)
                        {
                            int testX = cx + w + offsX + italicOffset;

                            if (testX >= width /* + italicOffset*/)
                            {
                                break;
                            }

                            int block = (testY * width) + testX;

                            if ((pData[block] == 0u) && pData[block] != blackColor)
                            {
                                int startX = (cx > 0) ? -1 : 0;
                                int startY = (cy > 0) ? -1 : 0;
                                int endX = (cx < dw - 1) ? 2 : 1;
                                int endY = (cy < dh - 1) ? 2 : 1;

                                if (endX == 2 && (testX + 1) >= width)
                                {
                                    endX--;
                                }

                                bool passed = false;

                                for (int x = startX; x < endX; x++)
                                {
                                    int nowX = testX + x;

                                    for (int y = startY; y < endY; y++)
                                    {
                                        int testBlock = ((testY + y) * width) + nowX;

                                        if ((pData[testBlock] != 0u) &&
                                            pData[testBlock] != blackColor)
                                        {
                                            pData[block] = blackColor;

                                            passed = true;

                                            break;
                                        }
                                    }

                                    if (passed)
                                    {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                w += (dw + offsX + (int)isSolid);
            }
            else if (si == L' ')
            {
                w += UNICODE_SPACE_WIDTH;

                if (m_UseHTML)
                {
                    isUnderline = ((dataPtr.flags & UOFONT_UNDERLINE) != 0);

                    if (dataPtr.color != 0xFFFFFFFF)
                    {
                        charcolor = dataPtr.color;
                        isBlackPixel =
                            (((charcolor >> 24) & 0xFF) <= 8 && ((charcolor >> 16) & 0xFF) <= 8 &&
                             ((charcolor >> 8) & 0xFF) <= 8);
                    }
                }
            }

            if (isUnderline)
            {
                int minXOk = ((tmpW + offsX) > 0) ? -1 : 0;
                int maxXOk = ((w + offsX + dw) < width) ? 1 : 0;

                uint8_t *aData = (uint8_t *)((size_t)table + table[L'a']);

                int testY = lineOffsY + (char)aData[1] + (char)aData[3];

                if (testY >= height)
                {
                    break;
                }

                for (int cx = minXOk; cx < dw + maxXOk; cx++)
                {
                    int testX = (cx + tmpW + offsX + (int)isSolid);

                    if (testX >= width)
                    {
                        break;
                    }

                    int block = (testY * width) + testX;

                    pData[block] = charcolor;
                }
            }
        }

        lineOffsY += ptr->MaxHeight;

        ptr = ptr->m_Next;

        info->Data.clear();
        delete info;
    }

    if (m_UseHTML && m_HTMLBackgroundCanBeColored && (m_BackgroundColor != 0u))
    {
        m_BackgroundColor |= 0xFF;

        for (int y = 0; y < height; y++)
        {
            int yPos = (y * width);

            for (int x = 0; x < width; x++)
            {
                uint32_t &p = pData[yPos + x];

                if (p == 0u)
                {
                    p = m_BackgroundColor;
                }
            }
        }
    }

    th.Width = width;
    th.Height = height;

    return pData;
}

bool CFontsManager::GenerateWBase(
    uint8_t font,
    CGLTextTexture &th,
    const wstring &str,
    uint16_t color,
    uint8_t cell,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    vector<uint32_t> pixels =
        GeneratePixelsW(font, th, str.c_str(), color, cell, width, align, flags);
    bool result = false;

    if (static_cast<unsigned int>(!pixels.empty()) != 0u)
    {
        g_GL_BindTexture32(th, th.Width, th.Height, &pixels[0]);

        result = true;
    }

    return result;
}

void CFontsManager::DrawW(
    uint8_t font,
    const wstring &str,
    uint16_t color,
    int x,
    int y,
    uint8_t cell,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    CGLTextTexture th;

    if (GenerateW(font, th, str, color, cell, width, align, flags))
    {
        th.Draw(x, y);
    }
}

uint8_t CFontsManager::m_FontIndex[256] = {
    0xFF, //0
    0xFF, //1
    0xFF, //2
    0xFF, //3
    0xFF, //4
    0xFF, //5
    0xFF, //6
    0xFF, //7
    0xFF, //8
    0xFF, //9
    0xFF, //10
    0xFF, //11
    0xFF, //12
    0xFF, //13
    0xFF, //14
    0xFF, //15
    0xFF, //16
    0xFF, //17
    0xFF, //18
    0xFF, //19
    0xFF, //20
    0xFF, //21
    0xFF, //22
    0xFF, //23
    0xFF, //24
    0xFF, //25
    0xFF, //26
    0xFF, //27
    0xFF, //28
    0xFF, //29
    0xFF, //30
    0xFF, //31
    0,    //32
    1,    //33
    2,    //34
    3,    //35
    4,    //36
    5,    //37
    6,    //38
    7,    //39
    8,    //40
    9,    //41
    10,   //42
    11,   //43
    12,   //44
    13,   //45
    14,   //46
    15,   //47
    16,   //48
    17,   //49
    18,   //50
    19,   //51
    20,   //52
    21,   //53
    22,   //54
    23,   //55
    24,   //56
    25,   //57
    26,   //58
    27,   //59
    28,   //60
    29,   //61
    30,   //62
    31,   //63
    32,   //64
    33,   //65
    34,   //66
    35,   //67
    36,   //68
    37,   //69
    38,   //70
    39,   //71
    40,   //72
    41,   //73
    42,   //74
    43,   //75
    44,   //76
    45,   //77
    46,   //78
    47,   //79
    48,   //80
    49,   //81
    50,   //82
    51,   //83
    52,   //84
    53,   //85
    54,   //86
    55,   //87
    56,   //88
    57,   //89
    58,   //90
    59,   //91
    60,   //92
    61,   //93
    62,   //94
    63,   //95
    64,   //96
    65,   //97
    66,   //98
    67,   //99
    68,   //100
    69,   //101
    70,   //102
    71,   //103
    72,   //104
    73,   //105
    74,   //106
    75,   //107
    76,   //108
    77,   //109
    78,   //110
    79,   //111
    80,   //112
    81,   //113
    82,   //114
    83,   //115
    84,   //116
    85,   //117
    86,   //118
    87,   //119
    88,   //120
    89,   //121
    90,   //122
    0xFF, //123
    0xFF, //124
    0xFF, //125
    0xFF, //126
    0xFF, //127
    0xFF, //128
    0xFF, //129
    0xFF, //130
    0xFF, //131
    0xFF, //132
    0xFF, //133
    0xFF, //134
    0xFF, //135
    0xFF, //136
    0xFF, //137
    0xFF, //138
    0xFF, //139
    0xFF, //140
    0xFF, //141
    0xFF, //142
    0xFF, //143
    0xFF, //144
    0xFF, //145
    0xFF, //146
    0xFF, //147
    0xFF, //148
    0xFF, //149
    0xFF, //150
    0xFF, //151
    0xFF, //152
    0xFF, //153
    0xFF, //154
    0xFF, //155
    0xFF, //156
    0xFF, //157
    0xFF, //158
    0xFF, //159
    0xFF, //160
    0xFF, //161
    0xFF, //162
    0xFF, //163
    0xFF, //164
    0xFF, //165
    0xFF, //166
    0xFF, //167
    136,  //168
    0xFF, //169
    0xFF, //170
    0xFF, //171
    0xFF, //172
    0xFF, //173
    0xFF, //174
    0xFF, //175
    0xFF, //176
    0xFF, //177
    0xFF, //178
    0xFF, //179
    0xFF, //180
    0xFF, //181
    0xFF, //182
    0xFF, //183
    152,  //184
    0xFF, //185
    0xFF, //186
    0xFF, //187
    0xFF, //188
    0xFF, //189
    0xFF, //190
    0xFF, //191
    160,  //192
    161,  //193
    162,  //194
    163,  //195
    164,  //196
    165,  //197
    166,  //198
    167,  //199
    168,  //200
    169,  //201
    170,  //202
    171,  //203
    172,  //204
    173,  //205
    174,  //206
    175,  //207
    176,  //208
    177,  //209
    178,  //210
    179,  //211
    180,  //212
    181,  //213
    182,  //214
    183,  //215
    184,  //216
    185,  //217
    186,  //218
    187,  //219
    188,  //220
    189,  //221
    190,  //222
    191,  //223
    192,  //224
    193,  //225
    194,  //226
    195,  //227
    196,  //228
    197,  //229
    198,  //230
    199,  //231
    200,  //232
    201,  //233
    202,  //234
    203,  //235
    204,  //236
    205,  //237
    206,  //238
    207,  //239
    208,  //240
    209,  //241
    210,  //242
    211,  //243
    212,  //244
    213,  //245
    214,  //246
    215,  //247
    216,  //248
    217,  //249
    218,  //250
    219,  //251
    220,  //252
    221,  //253
    222,  //254
    223   //255
};
