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
class WebRequestContext;

// This class handles communication between the IO thread where loading happens
// and the webkit main thread.
// TODO:
// - Implement didFail
// - Implement sync requests
// - Implement downloadFile
// - Implement pauseLoad
class WebUrlLoaderClient : public base::RefCountedThreadSafe<WebUrlLoaderClient> {
public:
    WebUrlLoaderClient(WebFrame*, WebCore::ResourceHandle*, const WebCore::ResourceRequest&);

    // Called from WebCore, will be forwarded to the IO thread
    bool start(bool isMainResource, bool isMainFrame, bool sync, WebRequestContext*);
    void cancel();
    void downloadFile();
    void pauseLoad(bool pause);
    void setAuth(const std::string& username, const std::string& password);
    void cancelAuth();
    void proceedSslCertError();
    void cancelSslCertError(int cert_error);

    typedef void CallbackFunction(void*);

    // This is called from the IO thread, and dispatches the callback to the main thread.
    // (For asynchronous calls, we just delegate to WebKit's callOnMainThread.)
    void maybeCallOnMainThread(Task* task);

    // Called by WebRequest (using maybeCallOnMainThread), should be forwarded to WebCore.
    void didReceiveResponse(PassOwnPtr<WebResponse>);
    void didReceiveData(scoped_refptr<net::IOBuffer>, int size);
    void didReceiveDataUrl(PassOwnPtr<std::string>);
    void didReceiveAndroidFileData(PassOwnPtr<std::vector<char> >);
    void didFinishLoading();
    void didFail(PassOwnPtr<WebResponse>);
    void willSendRequest(PassOwnPtr<WebResponse>);
    void authRequired(scoped_refptr<net::AuthChallengeInfo>, bool firstTime);
    void reportSslCertError(int cert_error, net::X509Certificate* cert);

    // Handle to the chrome IO thread
    static base::Thread* ioThread();

private:
    friend class base::RefCountedThreadSafe<WebUrlLoaderClient>;
    virtual ~WebUrlLoaderClient();

    void finish();

    WebFrame* m_webFrame;
    RefPtr<WebCore::ResourceHandle> m_resourceHandle;
    bool m_isMainResource;
    bool m_isMainFrame;
    bool m_isCertMimeType;
    bool m_cancelling;
    bool m_sync;
    volatile bool m_finished;

    scoped_refptr<WebRequest> m_request;
    OwnPtr<WebResponse> m_response; // NULL until didReceiveResponse is called.

    // Check if a request is active
    bool isActive() const;

    // Mutex and condition variable used for synchronous requests.
    // Note that these are static. This works because there's only one main thread.
    static Lock* syncLock();
    static ConditionVariable* syncCondition();

    // Queue of callbacks to be executed by the main thread. Must only be accessed inside mutex.
    std::deque<Task*> m_queue;
};

} // namespace android

#endif
