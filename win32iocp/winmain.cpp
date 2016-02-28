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

#include "CompletionPort.h"
#include "NonblockIoHandle.h"

int WSASocketPair(int domain, int type, int protocol, SOCKET socket_vector[2]);

static int process_error(int code)
{
    DWORD lastError = GetLastError();
    return code;
}

int client_thread_main(SOCKET s)
{
    class SocketClient : public NonblockIoHandle::Client {
    public:
        void handleDidClose(NonblockIoHandle*)
        {
            closed = true;
        }
        void handleDidRead(NonblockIoHandle*, size_t numberOfBytesTransferred)
        {
        }
        void handleDidWrite(NonblockIoHandle*, size_t numberOfBytesTransferred)
        {
            processing = false;
        }

        bool processing = false;
        bool closed = false;
    };

    SocketClient client;

    std::shared_ptr<CompletionPort> iocp = CompletionPort::create();
    std::shared_ptr<NonblockIoHandle> file = NonblockIoHandle::create(s, iocp, &client);

    static const char message[] = "Hello, World!";

    Sleep(500);

    if (file->write(message, sizeof(message)).first <= NonblockIoHandle::Pending)
        client.processing = true;

    while (client.processing) { }

    file->close();

    while (!client.closed) { }

    return process_error(0);
}

int _tmain(int argc, _TCHAR* argv[])
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return process_error(SOCKET_ERROR);

    SOCKET sv[2];
    if (WSASocketPair(AF_INET, SOCK_STREAM, IPPROTO_TCP, sv) == SOCKET_ERROR)
        return process_error(-1);

    std::thread client_thread(&client_thread_main, sv[1]);

    class SocketClient : public NonblockIoHandle::Client {
    public:
        void handleDidClose(NonblockIoHandle*)
        {
            closed = true;
        }
        void handleDidRead(NonblockIoHandle*, size_t numberOfBytesTransferred)
        {
            processing = false;
        }
        void handleDidWrite(NonblockIoHandle*, size_t numberOfBytesTransferred)
        {
        }

        bool processing = false;
        bool closed = false;
    };

    SocketClient client;

    std::shared_ptr<CompletionPort> iocp = CompletionPort::create();
    std::shared_ptr<NonblockIoHandle> file = NonblockIoHandle::create(sv[0], iocp, &client);

    static char message[256];

    if (file->read(message, 256).first <= NonblockIoHandle::Pending)
        client.processing = true;

    while (client.processing) { }

    std::cout << message << std::endl;

    file->close();

    while (!client.closed) { }
    client_thread.join();

    WSACleanup();

    getch();

    return process_error(0);
}
