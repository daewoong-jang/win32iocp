/*
 * Copyright (C) 2016 Daewoong Jang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "includes.h"

#include <future>

static const USHORT kLoopbackPort = 4444;

static int process_socket_error(int code)
{
    DWORD lastError = GetLastError();
    return code;
}

struct connect_thread_return {
    std::promise<SOCKET> result;
};

int connect_thread_main(connect_thread_return* r)
{
    SOCKET s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (s == INVALID_SOCKET) {
        r->result.set_value(SOCKET_ERROR);
        return process_socket_error(SOCKET_ERROR);
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(kLoopbackPort);

    if (WSAConnect(s, (sockaddr*)&addr, sizeof(sockaddr_in), NULL, NULL, NULL, NULL) == SOCKET_ERROR) {
        r->result.set_value(SOCKET_ERROR);
        return process_socket_error(SOCKET_ERROR);
    }

    r->result.set_value(s);

    return 0;
}

int WSASocketPair(int domain, int type, int protocol, SOCKET socket_vector[2])
{
    SOCKET s = WSASocket(domain, type, protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (s == INVALID_SOCKET)
        return process_socket_error(SOCKET_ERROR);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(sockaddr_in));
    addr.sin_family = domain;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(kLoopbackPort);

    if (bind(s, (sockaddr*)&addr, sizeof(sockaddr_in)) == SOCKET_ERROR)
        return process_socket_error(SOCKET_ERROR);

    if (listen(s, SOMAXCONN) == SOCKET_ERROR)
        return process_socket_error(SOCKET_ERROR);

    connect_thread_return r;
    std::thread connect_thread(&connect_thread_main, &r);

    SOCKET server = 0;
    if ((server = WSAAccept(s, NULL, 0, NULL, NULL)) == INVALID_SOCKET)
        return process_socket_error(SOCKET_ERROR);

    std::future<SOCKET> f = r.result.get_future();
    shutdown(s, SD_BOTH);
    closesocket(s);
    connect_thread.join();

    SOCKET client = f.get();
    if (client == SOCKET_ERROR)
        return process_socket_error(SOCKET_ERROR);

    socket_vector[0] = server;
    socket_vector[1] = client;

    return 0;
}
