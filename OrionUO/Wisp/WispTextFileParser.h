// MIT License

#pragma once

namespace Wisp
{
class CTextFileParser
{
public:
    string RawLine = "";

private:
    CMappedFile m_File;
    uint8_t *m_Ptr = 0;
    uint8_t *m_EOL = 0;
    uint8_t *m_End = 0;
    char m_Delimiters[50];
    int m_DelimitersSize = 0;
    char m_Comentaries[50];
    int m_ComentariesSize = 0;
    char m_Quotes[50];
    int m_QuotesSize = 0;
    bool m_Trim = false;

    void GetEOL();
    bool IsDelimiter();
    void SkipToData();
    bool IsComment();
    bool IsQuote();
    bool IsSecondQuote();
    string ObtainData();
    string ObtainQuotedData();
    void StartupInitalize(const char *delimiters, const char *comentaries, const char *quotes);
    void SaveRawLine();

public:
    CTextFileParser(
        const os_path &path,
        const char *delimiters = "",
        const char *comentaries = "",
        const char *quotes = "");
    ~CTextFileParser();

    void Restart();
    bool IsEOF();

    vector<string> ReadTokens(bool trim = true);
    vector<string> GetTokens(const char *str, bool trim = true);
};

class CTextFileWriter
{
private:
    FILE *m_File{ nullptr };

public:
    CTextFileWriter(const os_path &path);
    ~CTextFileWriter();

    bool Opened() { return (m_File != nullptr); };
    void Close();
    void WriteString(const string &key, const string &value);
    void WriteInt(const string &key, int value);
    void WriteBool(const string &key, bool value);
};

}; // namespace Wisp
