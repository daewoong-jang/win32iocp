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

#include "CompletionPort.h"

#include "NonblockIoHandle.h"

static const LPOVERLAPPED kPerformClose = (LPOVERLAPPED)1;

CompletionPort::CompletionPort()
    : m_port(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0))
    , m_error(0)
    , m_thread(&CompletionPort::threadMain, this)
{
    m_thread.detach();
}

CompletionPort::~CompletionPort()
{
    terminate();
}

bool CompletionPort::add(HANDLE fileHandle, std::shared_ptr<CompletionKey> completionKey)
{
    ASSERT(completionKey);

    if (m_error)
        return false;

    if (fileHandle == INVALID_HANDLE_VALUE)
        return false;

    ASSERT(m_keys.count(fileHandle) == 0);

    m_keys[fileHandle] = completionKey;

    if (CreateIoCompletionPort(fileHandle, m_port, reinterpret_cast<ULONG_PTR>(completionKey.get()), 0) == INVALID_HANDLE_VALUE) {
        handleError();
        return false;
    }

    return true;
}

bool CompletionPort::close(HANDLE fileHandle)
{
    ASSERT(fileHandle);
    ASSERT(m_keys.count(fileHandle) == 1);

    if (!PostQueuedCompletionStatus(m_port, 0, reinterpret_cast<ULONG_PTR>(m_keys[fileHandle].get()), kPerformClose)) {
        ASSERT_NOT_REACHED();
        handleError();
        return false;
    }

    return true;
}

void CompletionPort::didClose(HANDLE fileHandle)
{
    ASSERT(fileHandle);
    ASSERT(m_keys.count(fileHandle) == 1);

    m_keys.erase(fileHandle);
}

void CompletionPort::terminate()
{
    ASSERT(m_keys.size() == 0);

    CloseHandle(m_port);
}

int CompletionPort::threadMain()
{
#if (_WIN32_WINNT >= 0x0600)
    static const ULONG maxRemoveEntries = 256;
    ULONG removedEntries = 0;
    OVERLAPPED_ENTRY overlappedEntries[maxRemoveEntries];

    while (GetQueuedCompletionStatusEx(m_port, overlappedEntries, maxRemoveEntries, &removedEntries, INFINITE, TRUE)) {
        for (ULONG i = 0; i < removedEntries; ++i) {
            OVERLAPPED_ENTRY& entry = overlappedEntries[i];
            CompletionKey* completionKey = reinterpret_cast<CompletionKey*>(entry.lpCompletionKey);
            if (!completionKey)
                continue;

            if (entry.lpOverlapped == kPerformClose)
                completionKey->destroyKeyCallback();
            else
                completionKey->completionCallback(reinterpret_cast<CompletionStatus*>(entry.lpOverlapped), entry.dwNumberOfBytesTransferred);
        }
    }
#else
    DWORD numberOfBytesTransferred;
    ULONG_PTR statusCompletionKey;
    LPOVERLAPPED overlapped;

    while (GetQueuedCompletionStatus(m_port, &numberOfBytesTransferred, &statusCompletionKey, &overlapped, INFINITE)) {
        CompletionKey* completionKey = reinterpret_cast<CompletionKey*>(statusCompletionKey);
        if (!completionKey)
            continue;

        if (overlapped == kPerformClose)
            completionKey->destroyKeyCallback();
        else
            completionKey->completionCallback(reinterpret_cast<CompletionStatus*>(overlapped), numberOfBytesTransferred);
    }
#endif

    return 0;
}

void CompletionPort::handleError()
{
    m_error = GetLastError();
}
