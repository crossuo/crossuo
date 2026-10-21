// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <stdint.h>

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

