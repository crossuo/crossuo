/***********************************************************************************
**
** Connection.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef CONNECTION_H
#define CONNECTION_H

//!Класс менеджера подключения к серверу
class CSocket : public Wisp::CConnection
{
public:
    bool GameSocket = false;
    bool UseProxy = false;
    string ProxyAddress = "";
    int ProxyPort = 0;
    bool ProxySocks5 = false;
    string ProxyAccount = "";
    string ProxyPassword = "";

private:
    CDecompressingCopier m_Decompressor;

public:
    CSocket(bool gameSocket);
    ~CSocket();

    virtual bool Connect(const string &address, int port);

    virtual vector<uint8_t> Decompression(vector<uint8_t> data);
};

#endif