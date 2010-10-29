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

#ifndef WebRequest_h
#define WebRequest_h

#include "ChromiumIncludes.h"
#include "WebUrlLoaderClient.h"
#include "wtf/Vector.h"

class MessageLoop;

namespace android {

enum LoadState {
    Created,
    Started,
    Response,
    GotData,
    Cancelled,
    Finished,
    Deleted
};

class WebResourceRequest;
class WebFrame;

// All methods in this class must be called on the io thread
class WebRequest : public URLRequest::Delegate, public base::RefCountedThreadSafe<WebRequest> {
public:
    WebRequest(WebUrlLoaderClient*, const WebResourceRequest&);

    // If this is an android specific url we load it with a java input stream
    // Used for:
    // - file:///android_asset
    // - file:///android_res
    // - content://
    WebRequest(WebUrlLoaderClient*, const WebResourceRequest&, int inputStream);

    // Optional, but if used has to be called before start
    void AppendBytesToUpload(Vector<char>* data);

    void start(bool isPrivateBrowsing);
    void cancel();

    // From URLRequest::Delegate
    virtual void OnReceivedRedirect(URLRequest*, const GURL&, bool* deferRedirect);
    virtual void OnResponseStarted(URLRequest*);
    virtual void OnReadCompleted(URLRequest*, int bytesRead);
    virtual void OnAuthRequired(URLRequest*, net::AuthChallengeInfo*);

    // Methods called during a request by the UI code (via WebUrlLoaderClient).
    void setAuth(const string16& username, const string16& password);
    void cancelAuth();

    const std::string& getUrl() const;
    const std::string& getUserAgent() const;

private:
    void startReading();
    bool read(int* bytesRead);

    friend class base::RefCountedThreadSafe<WebRequest>;
    virtual ~WebRequest();
    void handleDataURL(GURL);
    void handleBrowserURL(GURL);
    void handleAndroidURL();
    void finish(bool success);

    scoped_refptr<WebUrlLoaderClient> m_urlLoader;
    OwnPtr<URLRequest> m_request;
    scoped_refptr<net::IOBuffer> m_networkBuffer;
    int m_inputStream;
    bool m_androidUrl;
    std::string m_url;
    std::string m_userAgent;
    LoadState m_loadState;
};

} // namespace android

#endif
