/*
 * Copyright 2011, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CacheResult.h"

#include "WebResponse.h"
#include "WebUrlLoaderClient.h"
#include <platform/FileSystem.h>
#include <wtf/text/CString.h>

using namespace base;
using namespace disk_cache;
using namespace net;
using namespace std;

namespace android {

// All public methods are called on a UI thread but we do work on the
// Chromium thread. However, because we block the WebCore thread while this
// work completes, we can never receive new public method calls while the
// Chromium thread work is in progress.

// Copied from HttpCache
enum {
    kResponseInfoIndex = 0,
    kResponseContentIndex
};

CacheResult::CacheResult(disk_cache::Entry* entry, String url)
    : m_entry(entry)
    , m_onResponseHeadersDoneCallback(this, &CacheResult::onResponseHeadersDone)
    , m_onReadNextChunkDoneCallback(this, &CacheResult::onReadNextChunkDone)
    , m_url(url)
{
    ASSERT(m_entry);
}

CacheResult::~CacheResult()
{
    m_entry->Close();
    // TODO: Should we also call DoneReadingFromEntry() on the cache for our
    // entry?
}

int64 CacheResult::contentSize() const
{
    // The android stack does not take the content length from the HTTP response
    // headers but calculates it when writing the content to disk. It can never
    // overflow a long because we limit the cache size.
    return m_entry->GetDataSize(kResponseContentIndex);
}

bool CacheResult::firstResponseHeader(const char* name, String* result, bool allowEmptyString) const
{
    string value;
    if (responseHeaders() && responseHeaders()->EnumerateHeader(NULL, name, &value) && (!value.empty() || allowEmptyString)) {
        *result = String(value.c_str());
        return true;
    }
    return false;
}

String CacheResult::mimeType() const
{
    string mimeType;
    if (responseHeaders())
        responseHeaders()->GetMimeType(&mimeType);
    if (!mimeType.length() && m_url.length())
        mimeType = WebResponse::resolveMimeType(std::string(m_url.utf8().data(), m_url.length()), "");
    return String(mimeType.c_str());
}

int64 CacheResult::expires() const
{
     // We have to do this manually, rather than using HttpResponseHeaders::GetExpiresValue(),
     // to handle the "-1" and "0" special cases.
     string expiresString;
     if (responseHeaders() && responseHeaders()->EnumerateHeader(NULL, "expires", &expiresString)) {
         wstring expiresStringWide(expiresString.begin(), expiresString.end());  // inflate ascii
         // We require the time expressed as ms since the epoch.
         Time time;
         if (Time::FromString(expiresStringWide.c_str(), &time)) {
             // Will not overflow for a very long time!
             return static_cast<int64>(1000.0 * time.ToDoubleT());
         }

         if (expiresString == "-1" || expiresString == "0")
             return 0;
     }

     // TODO
     // The Android stack applies a heuristic to set an expiry date if the
     // expires header is not set or can't be parsed. I'm  not sure whether the Chromium cache
     // does this, and if so, it may not be possible for us to get hold of it
     // anyway to set it on the result.
     return -1;
}

int CacheResult::responseCode() const
{
    return responseHeaders() ? responseHeaders()->response_code() : 0;
}

bool CacheResult::writeToFile(const String& filePath) const
{
    // Getting the headers is potentially async, so post to the Chromium thread
    // and block here.
    MutexLocker lock(m_mutex);

    base::Thread* thread = WebUrlLoaderClient::ioThread();
    if (!thread)
        return false;

    CacheResult* me = const_cast<CacheResult*>(this);
    thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(me, &CacheResult::writeToFileImpl));

    m_filePath = filePath.threadsafeCopy();
    m_isAsyncOperationInProgress = true;
    while (m_isAsyncOperationInProgress)
        m_condition.wait(m_mutex);

    return m_wasWriteToFileSuccessful;
}

void CacheResult::writeToFileImpl()
{
    m_bufferSize = m_entry->GetDataSize(kResponseContentIndex);
    m_readOffset = 0;
    m_wasWriteToFileSuccessful = false;
    readNextChunk();
}

void CacheResult::readNextChunk()
{
    m_buffer = new IOBuffer(m_bufferSize);
    int rv = m_entry->ReadData(kResponseInfoIndex, m_readOffset, m_buffer, m_bufferSize, &m_onReadNextChunkDoneCallback);
    if (rv == ERR_IO_PENDING)
        return;

    onReadNextChunkDone(rv);
};

void CacheResult::onReadNextChunkDone(int size)
{
    if (size > 0) {
        // Still more reading to be done.
        if (writeChunkToFile()) {
            // TODO: I assume that we need to clear and resize the buffer for the next read?
            m_readOffset += size;
            m_bufferSize -= size;
            readNextChunk();
        } else
            onWriteToFileDone();
        return;
    }

    if (!size) {
        // Reached end of file.
        if (writeChunkToFile())
            m_wasWriteToFileSuccessful = true;
    }
    onWriteToFileDone();
}

bool CacheResult::writeChunkToFile()
{
    PlatformFileHandle file;
    file = openFile(m_filePath, OpenForWrite);
    if (!isHandleValid(file))
        return false;
    return WebCore::writeToFile(file, m_buffer->data(), m_bufferSize) == m_bufferSize;
}

void CacheResult::onWriteToFileDone()
{
    MutexLocker lock(m_mutex);
    m_isAsyncOperationInProgress = false;
    m_condition.signal();
}

HttpResponseHeaders* CacheResult::responseHeaders() const
{
    MutexLocker lock(m_mutex);
    if (m_responseHeaders)
        return m_responseHeaders;

    // Getting the headers is potentially async, so post to the Chromium thread
    // and block here.
    base::Thread* thread = WebUrlLoaderClient::ioThread();
    if (!thread)
        return 0;

    CacheResult* me = const_cast<CacheResult*>(this);
    thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(me, &CacheResult::responseHeadersImpl));

    m_isAsyncOperationInProgress = true;
    while (m_isAsyncOperationInProgress)
        m_condition.wait(m_mutex);

    return m_responseHeaders;
}

void CacheResult::responseHeadersImpl()
{
    m_bufferSize = m_entry->GetDataSize(kResponseInfoIndex);
    m_buffer = new IOBuffer(m_bufferSize);

    int rv = m_entry->ReadData(kResponseInfoIndex, 0, m_buffer, m_bufferSize, &m_onResponseHeadersDoneCallback);
    if (rv == ERR_IO_PENDING)
        return;

    onResponseHeadersDone(rv);
};

void CacheResult::onResponseHeadersDone(int size)
{
    MutexLocker lock(m_mutex);
    // It's OK to throw away the HttpResponseInfo object as we hold our own ref
    // to the headers.
    HttpResponseInfo response;
    bool truncated = false; // TODO: Waht is this param for?
    if (size == m_bufferSize && HttpCache::ParseResponseInfo(m_buffer->data(), m_bufferSize, &response, &truncated))
        m_responseHeaders = response.headers;
    m_isAsyncOperationInProgress = false;
    m_condition.signal();
}

} // namespace android
