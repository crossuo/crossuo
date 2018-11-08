// MIT License

#pragma once

typedef struct tagmd5_state
{
    unsigned int count[2];
    unsigned int abcd[4];
    unsigned char buf[64];
} md5_state;

class MD5Crypt
{
public:
    MD5Crypt();
    ~MD5Crypt();

public:
    void Init(unsigned char *data, unsigned int size);
    void Encrypt(const unsigned char *in, unsigned char *out, int len);
    unsigned char *GetMD5() { return m_digest; }

protected:
    void process(md5_state *pms, const unsigned char *data);
    void start(md5_state *pms);
    void append(md5_state *pms, const unsigned char *data, int nbytes);
    void finish(md5_state *pms, unsigned char digest[16]);

    unsigned int m_tableIdx;
    unsigned char m_digest[16];
};
