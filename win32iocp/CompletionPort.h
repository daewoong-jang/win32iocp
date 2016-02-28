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
#include <unordered_map>

struct CompletionStatus : public OVERLAPPED {
    void* user;
};

class CompletionKey : protected std::enable_shared_from_this<CompletionKey> {
public:
    virtual void completionCallback(CompletionStatus*, size_t) = 0;
    virtual void destroyKeyCallback() = 0;
};

class CompletionPort final {
public:
    static std::shared_ptr<CompletionPort> create()
    {
        return std::shared_ptr<CompletionPort>(new CompletionPort);
    }
    ~CompletionPort();

    bool add(HANDLE, std::shared_ptr<CompletionKey>);
    bool close(HANDLE);

    void didClose(HANDLE);

    void terminate();

private:
    CompletionPort();

    int threadMain();

    void handleError();

    HANDLE m_port;
    DWORD m_error;
    std::thread m_thread;
    std::unordered_map<HANDLE, std::shared_ptr<CompletionKey>> m_keys;
};
