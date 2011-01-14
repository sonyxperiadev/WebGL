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

#ifndef CacheResult_h
#define CacheResult_h

#include "ChromiumIncludes.h"

#include <wtf/RefCounted.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/text/WTFString.h>

namespace android {

// A wrapper around a disk_cache::Entry. Provides fields appropriate for constructing a Java CacheResult object.
class CacheResult : public base::RefCountedThreadSafe<CacheResult> {
public:
    // Takes ownership of the Entry passed to the constructor.
    CacheResult(disk_cache::Entry*, String url);
    ~CacheResult();

    int64 contentSize() const;
    bool firstResponseHeader(const char* name, WTF::String* result, bool allowEmptyString) const;
    // The Android stack uses the empty string if no Content-Type headers are
    // found, so we use the same default here.
    WTF::String mimeType() const;
    // Returns the value of the expires header as milliseconds since the epoch.
    int64 expires() const;
    int responseCode() const;
    bool writeToFile(const WTF::String& filePath) const;
private:
    net::HttpResponseHeaders* responseHeaders() const;
    void responseHeadersImpl();
    void onResponseHeadersDone(int size);

    void writeToFileImpl();
    void readNextChunk();
    void onReadNextChunkDone(int size);
    bool writeChunkToFile();
    void onWriteToFileDone();

    disk_cache::Entry* m_entry;

    scoped_refptr<net::HttpResponseHeaders> m_responseHeaders;

    int m_readOffset;
    bool m_wasWriteToFileSuccessful;
    mutable String m_filePath;

    int m_bufferSize;
    scoped_refptr<net::IOBuffer> m_buffer;
    mutable bool m_isAsyncOperationInProgress;
    mutable WTF::Mutex m_mutex;
    mutable WTF::ThreadCondition m_condition;

    net::CompletionCallbackImpl<CacheResult> m_onResponseHeadersDoneCallback;
    net::CompletionCallbackImpl<CacheResult> m_onReadNextChunkDoneCallback;

    String m_url;
};

} // namespace android

#endif
