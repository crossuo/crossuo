// GPLv3 License
// Copyright (c) 2018 Danny Angelo Carminati Grein

#pragma once

extern bool g_DisablePing;

#if defined(XUO_WINDOWS)
#include <winsock.h>
#pragma comment(lib, "wsock32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
typedef struct hostent HOSTENT;
typedef HOSTENT *LPHOSTENT;
#define SOCKADDR struct sockaddr
#define SOCKADDR_IN struct in_addr
#define LPIN_ADDR struct in_addr *
#define LPSOCKADDR const SOCKADDR *
#endif

bool socket_init();
void socket_shutdown();
uint32_t socket_localaddress();

typedef void *tcp_socket;

tcp_socket tcp_open();
bool tcp_connect(tcp_socket socket, const char *address, uint16_t port);
int tcp_select(tcp_socket socket);
int tcp_recv(tcp_socket socket, unsigned char *data, size_t max_size);
int tcp_send(tcp_socket socket, unsigned char *data, size_t size);
void tcp_close(tcp_socket socket);

#if USE_PING

typedef void *icmp_handle;
icmp_handle icmp_open();
int icmp_query(icmp_handle handle, const char *ip, uint32_t *timems);
void icmp_close(icmp_handle handle);

#endif // USE_PING
