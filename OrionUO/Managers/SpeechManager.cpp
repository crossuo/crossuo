// MIT License
// Copyright (C) August 2016 Hotride

CSpeechManager g_SpeechManager;

CSpeechItem::CSpeechItem(uint16_t code, const wstring &data)
    : Code(code)
    , Data(data)
{
    if ((Data.length() != 0u) && Data[Data.length() - 1] == L'*')
    {
        CheckEnd = true;
        Data.resize(Data.length() - 1);
    }

    if ((Data.length() != 0u) && Data[0] == L'*')
    {
        CheckStart = true;
        Data.erase(Data.begin());
    }

    //LOG(L"[0x%04X]=(cs=%i, ce=%i) %s\n", m_Code, m_CheckStart, m_CheckEnd, m_Data.c_str());
}

CSpeechManager::CSpeechManager()
{
}

CSpeechManager::~CSpeechManager()
{
    DEBUG_TRACE_FUNCTION;
    m_SpeechEntries.clear();
    m_LangCodes.clear();
}

bool CSpeechManager::LoadSpeech()
{
    DEBUG_TRACE_FUNCTION;
    LoadLangCodes();

    string lang(255, 0);

    if (GetProfileStringA("intl", "sLanguage", "default", &lang[0], 4) == 0)
    {
        g_Language = m_LangCodes[0].Abbreviature;
    }
    else
    {
        g_Language = lang;
    }

    for (int i = 0; i < (int)m_LangCodes.size(); i++)
    {
        if (m_LangCodes[i].Abbreviature == g_Language)
        {
            CurrentLanguage = &m_LangCodes[i];
            break;
        }
    }

    if (CurrentLanguage == nullptr)
    {
        CurrentLanguage = &m_LangCodes[0];
        g_Language = m_LangCodes[0].Abbreviature;
    }

    LOG("Selected language: %s (%s)\n", g_Language.c_str(), lang.c_str());

    Wisp::CDataReader reader;
    vector<uint8_t> tempData;
    bool isUOP = false;

    if (g_FileManager.m_MainMisc.Start != nullptr)
    {
        CUopBlockHeader *block = g_FileManager.m_MainMisc.GetBlock(0x0891F809004D8081);

        if (block != nullptr)
        {
            tempData = g_FileManager.m_MainMisc.GetData(*block);
            reader.SetData(&tempData[0], tempData.size());
            isUOP = true;
        }
    }

    if (reader.Start == nullptr)
    {
        reader.SetData(g_FileManager.m_SpeechMul.Start, g_FileManager.m_SpeechMul.Size);
    }

    if (isUOP)
    {
        LOG("Loading speech from UOP\n");
        reader.Move(2);
        wstring mainData = reader.ReadWStringLE(reader.Size - 2);
        vector<wstring> list;
        wstring temp;
        for (const wchar_t &c : mainData)
        {
            if (c == 0x000D || c == 0x000A)
            {
                if (temp.length() != 0u)
                {
                    list.push_back(temp);
                    temp = {};
                }
            }
            else
            {
                temp.push_back(c);
            }
        }

        if (temp.length() != 0u)
        {
            list.push_back(temp);
            temp = {};
        }

        for (const wstring &line : list)
        {
            uint16_t code = 0xFFFF;
            temp = {};
            for (const wchar_t c : line)
            {
                if (c == 0x0009)
                {
                    if (temp.length() != 0u)
                    {
                        code = std::stoi(temp);
                        temp = {};
                    }
                }
                else
                {
                    temp.push_back(c);
                }
            }

            if ((temp.length() != 0u) && code != 0xFFFF)
            {
                m_SpeechEntries.push_back(CSpeechItem(code, temp));
            }
        }
    }
    else
    {
        LOG("Loading speech from MUL\n");
        while (!reader.IsEOF())
        {
            const uint16_t code = reader.ReadUInt16BE();
            const int len = reader.ReadUInt16BE();
            if (len == 0)
            {
                continue;
            }

            wstring str = DecodeUTF8(reader.ReadString(len));
            m_SpeechEntries.push_back(CSpeechItem(code, str));
        }
    }

    LOG("m_SpeechEntries.size()=%i\n", m_SpeechEntries.size());
    m_Loaded = true;
    return true;
}

bool CSpeechManager::LoadLangCodes()
{
    DEBUG_TRACE_FUNCTION;

    m_LangCodes.push_back(CLangCode("enu", 101, "English", "United States"));
    Wisp::CMappedFile &file = g_FileManager.m_LangcodeIff;

    auto _header = file.ReadString(36);
    (void)_header;
    while (!file.IsEOF())
    {
        CLangCode langCodeData;
        file.Move(4);

        uint32_t entryLen = file.ReadUInt32BE();
        langCodeData.Abbreviature = file.ReadString();
        langCodeData.Code = file.ReadUInt32LE();
        langCodeData.Language = file.ReadString();
        langCodeData.Country = file.ReadString();

        //длинна LangName и LangCountry + null terminator всегда являются четным количеством в файле.
        if (((langCodeData.Language.length() + langCodeData.Country.length() + 2) % 2) != 0u)
        {
            int nullTerminator = file.ReadUInt8();
            if (nullTerminator != 0)
            {
                throw "speechManager @ 138, invalid null terminator in langcodes.iff";
            }
        }

        m_LangCodes.push_back(langCodeData);
        //LOG("[0x%04X]: %s\n", langCodeData.Code, langCodeData.Abbreviature.c_str());
    }

    //if (m_LangCodes.size() != 135)
    //	return false;

    return true;
}

void CSpeechManager::GetKeywords(const wchar_t *text, vector<uint32_t> &codes)
{
    DEBUG_TRACE_FUNCTION;
    if (!m_Loaded || g_PacketManager.GetClientVersion() < CV_305D)
    { //Но по факту с 2.0.7 версии клиента
        return;
    }

    const auto size = (int)m_SpeechEntries.size();
    wstring input = ToLowerW(text);
    //to lower, case insensitive approach.

    for (int i = 0; i < size; i++)
    {
        CSpeechItem entry = m_SpeechEntries[i];
        wstring data = entry.Data;

        if (data.length() > input.length() || data.length() == 0)
        {
            continue;
        }

        if (!entry.CheckStart)
        {
            wstring start = input.substr(0, data.length());
            size_t hits = start.find(data);
            if (hits == wstring::npos)
            {
                continue;
            }
        }

        if (!entry.CheckEnd)
        {
            wstring end = input.substr(input.length() - data.length());
            size_t hits = end.find(data);
            if (hits == wstring::npos)
            {
                continue;
            }
        }

        size_t hits = input.find(data);
        if (hits != wstring::npos)
        {
            codes.push_back(entry.Code);
        }
    }
}
