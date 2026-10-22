// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "Sockets.h"
#include <sys/time.h> // struct timeval (public header; glibc-internal bits/ is not portable)
#include <cassert>
#include <common/str.h>
#include <cstdint>
#include <string.h>
#include <sys/select.h>

#define SOCKET_LOG(...)
#define SOCKET_DUMP(...)

#if defined(XUO_EMSC)
// ---------------------------------------------------------------------------
// WebSocket-based socket transport for emscripten.
//
// Browsers cannot open raw TCP sockets. The client connects to a WebSocket
// proxy (xuoproxy /ws/<host>/<port>) that bridges binary WebSocket frames
// to a TCP connection on the shard server.
//
// Each tcp_socket is a WebSocket context. tcp_connect builds the ws:// URL
// from the current page origin: ws://<origin>/ws/<host>/<port>
// ---------------------------------------------------------------------------

#include <emscripten.h>
#include <emscripten/websocket.h>
#include <emscripten/threading.h>

struct ws_context
{
    EMSCRIPTEN_WEBSOCKET_T handle = 0;
    bool connected = false;
    bool failed = false;
    bool closed = false;
    std::vector<uint8_t> recv_buf;
    size_t recv_pos = 0;
};

static EM_BOOL ws_on_open(int eventType, const EmscriptenWebSocketOpenEvent *e, void *userData)
{
    auto ctx = static_cast<ws_context *>(userData);
    ctx->connected = true;
    return 0;
}

static EM_BOOL ws_on_close(int eventType, const EmscriptenWebSocketCloseEvent *e, void *userData)
{
    auto ctx = static_cast<ws_context *>(userData);
    ctx->connected = false;
    ctx->closed = true;
    return 0;
}

static EM_BOOL ws_on_error(int eventType, const EmscriptenWebSocketErrorEvent *e, void *userData)
{
    auto ctx = static_cast<ws_context *>(userData);
    ctx->failed = true;
    ctx->connected = false;
    return 0;
}

static EM_BOOL ws_on_message(int eventType, const EmscriptenWebSocketMessageEvent *e, void *userData)
{
    auto ctx = static_cast<ws_context *>(userData);
    if (e->numBytes > 0 && e->data)
    {
        const auto *data = static_cast<const uint8_t *>(e->data);
        ctx->recv_buf.insert(ctx->recv_buf.end(), data, data + e->numBytes);
    }
    return 0;
}

static bool socket_init_done = false;

uint32_t socket_localaddress()
{
    return 0x7F000001; // 127.0.0.1
}

bool socket_init()
{
    if (!socket_init_done)
    {
        if (!emscripten_websocket_is_supported())
        {
            Error(Client, "WebSocket not supported");
            return false;
        }
        socket_init_done = true;
    }
    return true;
}

void socket_shutdown()
{
}

tcp_socket tcp_open()
{
    socket_init();
    auto ctx = new ws_context();
    return static_cast<tcp_socket>(ctx);
}

bool tcp_connect(tcp_socket socket, const char *address, uint16_t port)
{
    auto ctx = static_cast<ws_context *>(socket);
    if (!ctx)
        return false;

    // build the ws:// URL from the current page origin
    char url[512];
    EM_ASM(
    {
        var origin = self.location.origin.replace(/^http/, 'ws');
        var addr = UTF8ToString($0);
        var port = $1;
        var url = origin + '/ws/' + addr + '/' + port;
        stringToUTF8(url, $2, $3);
    },
    reinterpret_cast<intptr_t>(address), static_cast<int>(port),
    reinterpret_cast<intptr_t>(url), static_cast<int>(sizeof(url)));

    EmscriptenWebSocketCreateAttributes attr;
    emscripten_websocket_init_create_attributes(&attr);
    attr.url = url;
    attr.createOnMainThread = 0;

    ctx->handle = emscripten_websocket_new(&attr);
    if (ctx->handle <= 0)
    {
        Error(Client, "ws: create failed for %s", url);
        return false;
    }

    emscripten_websocket_set_onopen_callback(ctx->handle, ctx, ws_on_open);
    emscripten_websocket_set_onmessage_callback(ctx->handle, ctx, ws_on_message);
    emscripten_websocket_set_onclose_callback(ctx->handle, ctx, ws_on_close);
    emscripten_websocket_set_onerror_callback(ctx->handle, ctx, ws_on_error);

    // the connection is async; yield to the event loop until the onopen
    // callback fires (emscripten_thread_sleep processes worker events)
    int wait = 0;
    while (!ctx->connected && !ctx->failed && wait < 10000)
    {
        emscripten_thread_sleep(10);
        wait += 10;
    }

    if (!ctx->connected)
    {
        Error(Client, "ws: connection timeout for %s", url);
        return false;
    }
    return true;
}

int tcp_select(tcp_socket socket)
{
    auto ctx = static_cast<ws_context *>(socket);
    if (!ctx)
        return 0;
    // A closed WebSocket must not hide buffered data. A server that writes
    // its final packet(s) and immediately closes (the login server sends the
    // 0x8C relay reply and drops the connection) delivers the binary frame
    // and the close event to the worker in the same event-loop turn; gating
    // readability on ctx->connected loses that tail - the 0x8C stays in
    // recv_buf forever, is never dispatched, and the client stalls at the
    // server-pick screen (intermittently, depending on whether the browser
    // happens to split the two events across turns; a native select()
    // reports a closed-with-data socket readable, which is the contract
    // CConnection expects). Model the native EOF: buffered bytes remain
    // readable, and -1 (error) is reported only once the buffer has drained.
    if (ctx->recv_buf.size() > ctx->recv_pos)
        return 1;
    if (ctx->closed || ctx->failed)
        return -1;
    return 0;
}

int tcp_recv(tcp_socket socket, unsigned char *data, size_t max_size)
{
    auto ctx = static_cast<ws_context *>(socket);
    if (!ctx)
        return -1;

    size_t available = ctx->recv_buf.size() - ctx->recv_pos;
    if (available == 0)
    {
        // drained (or never filled): EOF when the socket ended, plain
        // "no data" while it is still live
        return (ctx->closed || ctx->failed) ? 0 : (ctx->connected ? 0 : -1);
    }

    size_t to_copy = std::min(max_size, available);
    memcpy(data, ctx->recv_buf.data() + ctx->recv_pos, to_copy);
    ctx->recv_pos += to_copy;

    if (ctx->recv_pos >= ctx->recv_buf.size())
    {
        ctx->recv_buf.clear();
        ctx->recv_pos = 0;
    }
    return static_cast<int>(to_copy);
}

int tcp_send(tcp_socket socket, unsigned char *data, size_t size)
{
    auto ctx = static_cast<ws_context *>(socket);
    if (!ctx || !ctx->connected)
        return -1;

    EMSCRIPTEN_RESULT res =
        emscripten_websocket_send_binary(ctx->handle, data, static_cast<uint32_t>(size));
    if (res != EMSCRIPTEN_RESULT_SUCCESS)
    {
        Error(Client, "ws: send failed (%d)", static_cast<int>(res));
        return -1;
    }
    return static_cast<int>(size);
}

void tcp_close(tcp_socket socket)
{
    auto ctx = static_cast<ws_context *>(socket);
    if (!ctx)
        return;
    if (ctx->handle > 0)
    {
        emscripten_websocket_close(ctx->handle, 1000, "normal");
        emscripten_websocket_delete(ctx->handle);
    }
    delete ctx;
}

#else // !XUO_EMSC — native implementation

#include <common/str.h>
#ifdef _WIN32
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define closesocket close
#endif

template<class T>
SOCKET socket_fd(T socket)
{
    return *reinterpret_cast<SOCKET *>(socket);
}

uint32_t socket_localaddress()
{
    char host[256];
    gethostname(host, sizeof(host));
    struct hostent *he = gethostbyname(host);
    if (he == nullptr)
        return 0x7F000001;
    return *reinterpret_cast<uint32_t *>(he->h_addr);
}

bool socket_init()
{
#ifdef _WIN32
    static bool wsa_initialized = false;
    if (!wsa_initialized)
    {
        WSADATA wsaData = {};
        wsa_initialized = WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
    }
    return wsa_initialized;
#else
    return true;
#endif
}

void socket_shutdown()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

tcp_socket tcp_open()
{
    socket_init();
    auto h = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (h == INVALID_SOCKET)
        return nullptr;
    auto *s = new SOCKET;
    *s = h;
    return reinterpret_cast<tcp_socket>(s);
}

bool tcp_connect(tcp_socket socket, const char *address, uint16_t port)
{
    auto h = socket_fd(socket);
    sockaddr_in caddr;
    memset(&caddr, 0, sizeof(sockaddr_in));
    caddr.sin_family = AF_INET;

    struct hostent *he;
    int rt = inet_addr(address);
    if (rt != -1)
    {
        caddr.sin_addr.s_addr = rt;
    }
    else
    {
        he = gethostbyname(address);
        if (he == nullptr)
            return false;
        memcpy(&caddr.sin_addr, he->h_addr, he->h_length);
    }

    caddr.sin_port = htons(port);
    if (connect(h, reinterpret_cast<struct sockaddr *>(&caddr), sizeof(caddr)) == -1)
        return false;
    return true;
}

int tcp_select(tcp_socket socket)
{
    auto h = socket_fd(socket);
    fd_set rfds;
    struct timeval tv = { 0, 0 };
    FD_ZERO(&rfds);
    FD_SET(h, &rfds);
    return select(static_cast<int>(h) + 1, &rfds, nullptr, nullptr, &tv);
}

int tcp_recv(tcp_socket socket, unsigned char *data, size_t max_size)
{
    auto h = socket_fd(socket);
    return static_cast<int>(recv(h, reinterpret_cast<char *>(data), static_cast<int>(max_size), 0));
}

int tcp_send(tcp_socket socket, unsigned char *data, size_t size)
{
    auto h = socket_fd(socket);
    return static_cast<int>(send(h, reinterpret_cast<const char *>(data), static_cast<int>(size), 0));
}

void tcp_close(tcp_socket socket)
{
    auto h = socket_fd(socket);
    closesocket(h);
    delete reinterpret_cast<SOCKET *>(socket);
}

#endif // !XUO_EMSC
