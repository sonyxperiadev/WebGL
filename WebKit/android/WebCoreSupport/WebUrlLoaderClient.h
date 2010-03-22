/*
 * Copyright 2010, The Android Open Source Project
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

#ifndef WebUrlLoaderClient_h
#define WebUrlLoaderClient_h

#include "RefCounted.h"
#include "WebUrlLoader.h"

#include "base/ref_counted.h"
#include <string>
#include <vector>

namespace base {
class Thread;
}

namespace net {
class IOBuffer;
}

namespace android {

class WebRequest;

// This class handles communication between the IO thread where loading happens
// and the webkit main thread.
// TODO:
// - Corner case where this gets deleted before UrlRequestAndroid
// - Implement didFail
// - Implement sync requests
// - Implement downloadFile
// - Implement pauseLoad
class WebUrlLoaderClient {
public:
    WebUrlLoaderClient(WebCore::ResourceHandle*, const WebCore::ResourceRequest&);
    ~WebUrlLoaderClient();

    // Called from WebCore, will be forwarded to the IO thread
    bool start(bool sync);
    void cancel();
    void downloadFile();
    void pauseLoad(bool pause) {} // Android method, does nothing for now

    // Called by WebRequest, should be forwarded to WebCore
    static void didReceiveResponse(void*);
    static void didReceiveData(void*);
    static void didReceiveDataUrl(void*);
    static void didFinishLoading(void*);
    static void didFail(void*);
    static void willSendRequest(void*);

private:
    void finish();
    RefPtr<WebCore::ResourceHandle> m_resourceHandle;
    WebCore::ResourceRequest m_resourceRequest;
    bool m_cancelling;

    // Not an OwnPtr since it should be deleted on another thread
    WebRequest* m_request;

    // Handle to the chrome IO thread
    static base::Thread* ioThread();

    // Vector containing all active loader clients
    static std::vector<WebUrlLoaderClient*>* activeLoaders;

    static bool isActive(const WebUrlLoaderClient*);
};

// A struct to send more than one thing in a void*, needed for callOnMainThread
struct LoaderData {
    net::IOBuffer* m_buffer;
    WebUrlLoaderClient* m_loader;
    OwnPtr<WebCore::ResourceResponse*> m_resourceResponse;
    const int m_size;
    OwnPtr<std::string*> m_string;

    LoaderData(WebUrlLoaderClient* l) : m_buffer(0), m_loader(l), m_resourceResponse(0), m_size(0), m_string(0)
    {
    }

    LoaderData(WebUrlLoaderClient* l, std::string* s) : m_buffer(0), m_loader(l), m_resourceResponse(0), m_size(0), m_string(s)
    {
    }

    LoaderData(WebUrlLoaderClient* l, WebCore::ResourceResponse* r) : m_buffer(0), m_loader(l), m_resourceResponse(r), m_size(0), m_string(0)
    {
    }

    LoaderData(WebUrlLoaderClient* l, net::IOBuffer* b, const int s) : m_buffer(b), m_loader(l), m_resourceResponse(0), m_size(s), m_string(0)
    {
    }

    ~LoaderData();
};

} // namespace android

#endif
