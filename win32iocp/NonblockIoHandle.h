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

#pragma once

#include "includes.h"
#include "CompletionPort.h"
#include <winsock2.h>

class NonblockIoHandle final : public CompletionKey {
public:
    enum Operation { Read, Write };
    enum ErrorCode { Complete, Pending, Shutdown, InvalidOperation, UnhandledError };

    class Client {
    public:
        virtual void handleDidClose(NonblockIoHandle*) = 0;
        virtual void handleDidRead(NonblockIoHandle*, size_t) = 0;
        virtual void handleDidWrite(NonblockIoHandle*, size_t) = 0;
    };

    static std::shared_ptr<NonblockIoHandle> create(HANDLE, std::shared_ptr<CompletionPort>, Client*);
    static std::shared_ptr<NonblockIoHandle> create(SOCKET, std::shared_ptr<CompletionPort>, Client*);
    ~NonblockIoHandle();

    std::pair<ErrorCode, size_t> read(void*, size_t);
    std::pair<ErrorCode, size_t> write(const void*, size_t);

    void close();

private:
    NonblockIoHandle(HANDLE, std::shared_ptr<CompletionPort>, Client*);
    NonblockIoHandle(SOCKET, std::shared_ptr<CompletionPort>, Client*);

    static std::shared_ptr<NonblockIoHandle> activate(std::shared_ptr<NonblockIoHandle>);

    void closeNow();

    CompletionStatus* allocateCompletionStatus(Operation);
    void freeCompletionStatus(CompletionStatus*);

    void completionCallback(CompletionStatus*, size_t) override;
    void destroyKeyCallback() override;

    std::pair<ErrorCode, size_t> handleError(Operation, size_t);

    HANDLE m_handle;
    bool m_isSocket;
    std::shared_ptr<CompletionPort> m_port;
    Client* m_client;
    bool m_closing;
};
