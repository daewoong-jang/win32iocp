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

#include "NonblockIoHandle.h"

#include <mutex>

std::shared_ptr<NonblockIoHandle> NonblockIoHandle::create(HANDLE handle, std::shared_ptr<CompletionPort> port, Client* client)
{
    return activate(std::shared_ptr<NonblockIoHandle>(new NonblockIoHandle(handle, port, client)));
}

std::shared_ptr<NonblockIoHandle> NonblockIoHandle::create(SOCKET socket, std::shared_ptr<CompletionPort> port, Client* client)
{
    return activate(std::shared_ptr<NonblockIoHandle>(new NonblockIoHandle(socket, port, client)));
}

NonblockIoHandle::NonblockIoHandle(HANDLE handle, std::shared_ptr<CompletionPort> port, Client* client)
    : m_handle(handle)
    , m_isSocket(false)
    , m_port(port)
    , m_client(client)
    , m_closing(false)
{
    ASSERT(handle && handle != INVALID_HANDLE_VALUE);
    ASSERT(m_client);
    ASSERT(m_port);
}

NonblockIoHandle::NonblockIoHandle(SOCKET socket, std::shared_ptr<CompletionPort> port, Client* client)
    : m_handle((HANDLE)socket)
    , m_isSocket(true)
    , m_port(port)
    , m_client(client)
    , m_closing(false)
{
    ASSERT(socket && socket != INVALID_SOCKET);
    ASSERT(m_client);
    ASSERT(m_port);
}

NonblockIoHandle::~NonblockIoHandle()
{
    ASSERT(!m_handle);
    close();
}

std::shared_ptr<NonblockIoHandle> NonblockIoHandle::activate(std::shared_ptr<NonblockIoHandle> file)
{
    if (!file->m_port->add(file->m_handle, file))
        CRASH();

    return file;
}

std::pair<NonblockIoHandle::ErrorCode, size_t> NonblockIoHandle::read(void* buffer, size_t bufferSize)
{
    ASSERT(!m_closing);

    if (!buffer || bufferSize == 0)
        return std::make_pair(InvalidOperation, 0);

    DWORD bytesRead = 0;
    if (!ReadFile(m_handle, buffer, bufferSize, &bytesRead, allocateCompletionStatus(Read)))
        return handleError(Read, bytesRead);

    return std::make_pair(Complete, bytesRead);
}

std::pair<NonblockIoHandle::ErrorCode, size_t> NonblockIoHandle::write(const void* buffer, size_t bufferSize)
{
    ASSERT(!m_closing);

    if (!buffer || bufferSize == 0)
        return std::make_pair(InvalidOperation, 0);

    DWORD bytesSent = 0;
    if (!WriteFile(m_handle, buffer, bufferSize, &bytesSent, allocateCompletionStatus(Write)))
        return handleError(Write, bytesSent);

    return std::make_pair(Complete, bytesSent);
}

void NonblockIoHandle::close()
{
    if (m_closing ||!m_handle)
        return;

    m_closing = true;

    if (!m_port->close(m_handle))
        m_closing = false;
}

void NonblockIoHandle::closeNow()
{
    ASSERT(m_closing);

    if (m_isSocket)
        closesocket((SOCKET)m_handle);
    else
        CloseHandle(m_handle);

    m_handle = 0;
    m_closing = false;
}

CompletionStatus* NonblockIoHandle::allocateCompletionStatus(Operation operation)
{
    CompletionStatus* status = new CompletionStatus;
    memset(status, 0, sizeof(CompletionStatus));
    status->user = reinterpret_cast<void*>(static_cast<int>(operation));
    return status;
}

void NonblockIoHandle::freeCompletionStatus(CompletionStatus* status)
{
    delete status;
}

void NonblockIoHandle::completionCallback(CompletionStatus* passedStatus, size_t bytesTransferred)
{
    CompletionStatus status(*passedStatus);
    freeCompletionStatus(passedStatus);

    Operation operation = static_cast<Operation>(reinterpret_cast<int>(status.user));

    DWORD numberOfBytesTransferred = 0;
    while (!::GetOverlappedResult(m_handle, &status, &numberOfBytesTransferred, FALSE)) {
        switch (DWORD error = GetLastError()) {
        case ERROR_BROKEN_PIPE:
        case WSAECONNRESET:
        case WSAESHUTDOWN:
            m_client->handleDidClose(this);
            return;
        case ERROR_IO_INCOMPLETE:
        case ERROR_IO_PENDING:
        default:
            ASSERT_NOT_REACHED();
            return;
        }
    }

    switch (operation) {
    case NonblockIoHandle::Read:
        m_client->handleDidRead(this, numberOfBytesTransferred);
        break;
    case NonblockIoHandle::Write:
        m_client->handleDidWrite(this, numberOfBytesTransferred);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void NonblockIoHandle::destroyKeyCallback()
{
    HANDLE closingHandle = m_handle;
    Client* closingClient = m_client;
    closeNow();
    m_port->didClose(closingHandle);
    closingClient->handleDidClose(this);
}

std::pair<NonblockIoHandle::ErrorCode, size_t> NonblockIoHandle::handleError(Operation operation, size_t size)
{
    switch (DWORD error = GetLastError()) {
    case ERROR_IO_PENDING:
        return std::make_pair(Pending, size);
    case ERROR_BROKEN_PIPE:
    case WSAECONNRESET:
    case WSAESHUTDOWN:
        return std::make_pair(Shutdown, size);
    case ERROR_IO_INCOMPLETE:
    default:
        return std::make_pair(UnhandledError, error);
    }
}
