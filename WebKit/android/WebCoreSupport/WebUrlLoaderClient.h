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

#include "ChromiumIncludes.h"
#include "RefCounted.h"
#include "WebResponse.h"
#include "WebUrlLoader.h"

#include <string>
#include <deque>
#include <string>
#include <vector>

class Lock;
class ConditionVariable;

namespace base {
class Thread;
}

namespace net {
class IOBuffer;
class AuthChallengeInfo;
}

namespace android {

class WebFrame;
class WebRequest;

// This class handles communication between the IO thread where loading happens
// and the webkit main thread.
// TODO:
// - Implement didFail
// - Implement sync requests
// - Implement downloadFile
// - Implement pauseLoad
class WebUrlLoaderClient {
public:
    WebUrlLoaderClient(WebFrame*, WebCore::ResourceHandle*, const WebCore::ResourceRequest&);
    ~WebUrlLoaderClient();

    // Called from WebCore, will be forwarded to the IO thread
    bool start(bool sync, bool isPrivateBrowsing);
    void cancel();
    void downloadFile();
    void pauseLoad(bool pause) {} // Android method, does nothing for now
    void setAuth(const std::string& username, const std::string& password);
    void cancelAuth();

    typedef void CallbackFunction(void*);

    // This is called from the IO thread, and dispatches the callback to the main thread.
    // (For asynchronous calls, we just delegate to WebKit's callOnMainThread.)
    void maybeCallOnMainThread(CallbackFunction*, void* context);

    // Called by WebRequest (using maybeCallOnMainThread), should be forwarded to WebCore.
    static void didReceiveResponse(void*);
    static void didReceiveData(void*);
    static void didReceiveDataUrl(void*);
    static void didReceiveAndroidFileData(void*);
    static void didFinishLoading(void*);
    static void didFail(void*);
    static void willSendRequest(void*);
    static void authRequired(void*);

    // Handle to the chrome IO thread
    static base::Thread* ioThread();

private:
    void finish();

    WebFrame* m_webFrame;
    RefPtr<WebCore::ResourceHandle> m_resourceHandle;
    bool m_cancelling;
    bool m_sync;
    volatile bool m_finished;

    // Not an OwnPtr since it should be deleted on another thread
    WebRequest* m_request;

    // Check if a request is active
    bool isActive() const;

    // Mutex and condition variable used for synchronous requests.
    // Note that these are static. This works because there's only one main thread.
    static Lock* syncLock();
    static ConditionVariable* syncCondition();

    typedef Tuple2<CallbackFunction*, void*> Callback;
    typedef std::deque<Callback> CallbackQueue;

    // Queue of callbacks to be executed by the main thread. Must only be accessed inside mutex.
    CallbackQueue m_queue;
};

// A struct to send more than one thing in a void*, needed for maybeCallOnMainThread
struct LoaderData {
    net::IOBuffer* buffer;
    WebUrlLoaderClient* loader;
    WebResponse webResponse;
    const int size;
    OwnPtr<std::string*> string;
    OwnPtr<std::vector<char> > vector;
    scoped_refptr<net::AuthChallengeInfo> authChallengeInfo;

    LoaderData(WebUrlLoaderClient* l) : buffer(0), loader(l), size(0)
    {
    }

    LoaderData(WebUrlLoaderClient* l, std::string* s) : buffer(0), loader(l), size(0), string(s)
    {
    }

    LoaderData(WebUrlLoaderClient* l, WebResponse r) : buffer(0), loader(l), webResponse(r), size(0)
    {
    }

    LoaderData(WebUrlLoaderClient* l, net::IOBuffer* b, const int s) : buffer(b), loader(l), size(s)
    {
    }

    LoaderData(WebUrlLoaderClient* l, std::vector<char>* data, const int s) : buffer(0), loader(l), size(s), vector(data)
    {
    }

    ~LoaderData();
};

} // namespace android

#endif
