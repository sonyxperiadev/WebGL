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

#include "config.h"
#include "WebRequest.h"

#include "JNIUtility.h"
#include "MainThread.h"
#include "UrlInterceptResponse.h"
#include "WebCoreFrameBridge.h"
#include "WebRequestContext.h"
#include "WebResourceRequest.h"
#include "WebUrlLoaderClient.h"
#include "jni.h"

#include <cutils/log.h>
#include <string>
#include <utils/AssetManager.h>

extern android::AssetManager* globalAssetManager();

// TODO:
// - Finish the file upload. Testcase is mobile buzz
// - Add network throttle needed by Android plugins

// TODO: Turn off asserts crashing before release
// http://b/issue?id=2951985
#undef ASSERT
#define ASSERT(assertion, ...) do \
    if (!(assertion)) { \
        android_printLog(ANDROID_LOG_ERROR, __FILE__, __VA_ARGS__); \
    } \
while (0)

namespace android {

namespace {
    const int kInitialReadBufSize = 32768;
}

WebRequest::WebRequest(WebUrlLoaderClient* loader, const WebResourceRequest& webResourceRequest)
    : m_urlLoader(loader)
    , m_androidUrl(false)
    , m_url(webResourceRequest.url())
    , m_userAgent(webResourceRequest.userAgent())
    , m_loadState(Created)
    , m_authRequestCount(0)
    , m_cacheMode(0)
    , m_runnableFactory(this)
    , m_wantToPause(false)
    , m_isPaused(false)
    , m_isSync(false)
{
    GURL gurl(m_url);

    m_request = new URLRequest(gurl, this);

    m_request->SetExtraRequestHeaders(webResourceRequest.requestHeaders());
    m_request->set_referrer(webResourceRequest.referrer());
    m_request->set_method(webResourceRequest.method());
    m_request->set_load_flags(webResourceRequest.loadFlags());
}

// This is a special URL for Android. Query the Java InputStream
// for data and send to WebCore
WebRequest::WebRequest(WebUrlLoaderClient* loader, const WebResourceRequest& webResourceRequest, UrlInterceptResponse* intercept)
    : m_urlLoader(loader)
    , m_interceptResponse(intercept)
    , m_androidUrl(true)
    , m_url(webResourceRequest.url())
    , m_userAgent(webResourceRequest.userAgent())
    , m_loadState(Created)
    , m_authRequestCount(0)
    , m_cacheMode(0)
    , m_runnableFactory(this)
    , m_wantToPause(false)
    , m_isPaused(false)
    , m_isSync(false)
{
}

WebRequest::~WebRequest()
{
    ASSERT(m_loadState == Finished, "dtor called on a WebRequest in a different state than finished (%d)", m_loadState);

    m_loadState = Deleted;
}

const std::string& WebRequest::getUrl() const
{
    return m_url;
}

const std::string& WebRequest::getUserAgent() const
{
    return m_userAgent;
}

#ifdef LOG_REQUESTS
namespace {
int remaining = 0;
}
#endif

void WebRequest::finish(bool success)
{
    m_runnableFactory.RevokeAll();
    ASSERT(m_loadState < Finished, "(%p) called finish on an already finished WebRequest (%d) (%s)", this, m_loadState, m_url.c_str());
    if (m_loadState >= Finished)
        return;
#ifdef LOG_REQUESTS
    time_t finish;
    time(&finish);
    finish = finish - m_startTime;
    struct tm * timeinfo;
    char buffer[80];
    timeinfo = localtime(&finish);
    strftime(buffer, 80, "Time: %M:%S",timeinfo);
    android_printLog(ANDROID_LOG_DEBUG, "KM", "(%p) finish (%d) (%s) (%d) (%s)", this, --remaining, buffer, success, m_url.c_str());
#endif

    // Make sure WebUrlLoaderClient doesn't delete us in the middle of this method.
    scoped_refptr<WebRequest> guard(this);

    m_loadState = Finished;
    if (success) {
        m_urlLoader->maybeCallOnMainThread(NewRunnableMethod(
                m_urlLoader.get(), &WebUrlLoaderClient::didFinishLoading));
    } else {
        if (m_interceptResponse == NULL) {
            OwnPtr<WebResponse> webResponse(new WebResponse(m_request.get()));
            m_urlLoader->maybeCallOnMainThread(NewRunnableMethod(
                    m_urlLoader.get(), &WebUrlLoaderClient::didFail, webResponse.release()));
        } else {
            OwnPtr<WebResponse> webResponse(new WebResponse(m_url, m_interceptResponse->mimeType(), 0,
                    m_interceptResponse->encoding(), m_interceptResponse->status()));
            m_urlLoader->maybeCallOnMainThread(NewRunnableMethod(
                    m_urlLoader.get(), &WebUrlLoaderClient::didFail, webResponse.release()));
        }
    }
    m_networkBuffer = 0;
    m_request = 0;
    m_urlLoader = 0;
}

void WebRequest::appendFileToUpload(const std::string& filename)
{
    // AppendFileToUpload is only valid before calling start
    ASSERT(m_loadState == Created, "appendFileToUpload called on a WebRequest not in CREATED state: (%s)", m_url.c_str());
    FilePath filePath(filename);
    m_request->AppendFileToUpload(filePath);
}

void WebRequest::appendBytesToUpload(WTF::Vector<char>* data)
{
    // AppendBytesToUpload is only valid before calling start
    ASSERT(m_loadState == Created, "appendBytesToUpload called on a WebRequest not in CREATED state: (%s)", m_url.c_str());
    m_request->AppendBytesToUpload(data->data(), data->size());
    delete data;
}

void WebRequest::setRequestContext(WebRequestContext* context)
{
    m_cacheMode = context->getCacheMode();
    if (m_request)
        m_request->set_context(context);
}

void WebRequest::updateLoadFlags(int& loadFlags)
{
    if (m_cacheMode == 1) { // LOAD_CACHE_ELSE_NETWORK
        loadFlags |= net::LOAD_PREFERRING_CACHE;
        loadFlags &= ~net::LOAD_VALIDATE_CACHE;
    }
    if (m_cacheMode == 2) // LOAD_NO_CACHE
        loadFlags |= net::LOAD_BYPASS_CACHE;
    if (m_cacheMode == 3) // LOAD_CACHE_ONLY
        loadFlags |= net::LOAD_ONLY_FROM_CACHE;

    if (m_isSync)
        loadFlags |= net::LOAD_IGNORE_LIMITS;
}

void WebRequest::start()
{
    ASSERT(m_loadState == Created, "Start called on a WebRequest not in CREATED state: (%s)", m_url.c_str());
#ifdef LOG_REQUESTS
    android_printLog(ANDROID_LOG_DEBUG, "KM", "(%p) start (%d) (%s)", this, ++remaining, m_url.c_str());
    time(&m_startTime);
#endif

    m_loadState = Started;

    if (m_interceptResponse != NULL)
        return handleInterceptedURL();

    // Handle data urls before we send it off to the http stack
    if (m_request->url().SchemeIs("data"))
        return handleDataURL(m_request->url());

    if (m_request->url().SchemeIs("browser"))
        return handleBrowserURL(m_request->url());

    // Update load flags with settings from WebSettings
    int loadFlags = m_request->load_flags();
    updateLoadFlags(loadFlags);
    m_request->set_load_flags(loadFlags);

    m_request->Start();
}

void WebRequest::cancel()
{
    ASSERT(m_loadState >= Started, "Cancel called on a not started WebRequest: (%s)", m_url.c_str());
    ASSERT(m_loadState != Cancelled, "Cancel called on an already cancelled WebRequest: (%s)", m_url.c_str());

    // There is a possible race condition between the IO thread finishing the request and
    // the WebCore thread cancelling it. If the request has already finished, do
    // nothing to avoid sending duplicate finish messages to WebCore.
    if (m_loadState > Cancelled) {
        return;
    }
    ASSERT(m_request, "Request set to 0 before it is finished");

    m_loadState = Cancelled;

    m_request->Cancel();
    finish(true);
}

void WebRequest::pauseLoad(bool pause)
{
    ASSERT(m_loadState >= GotData, "PauseLoad in state other than RESPONSE and GOTDATA");
    if (pause) {
        if (!m_isPaused)
            m_wantToPause = true;
    } else {
        m_wantToPause = false;
        if (m_isPaused) {
            m_isPaused = false;
            MessageLoop::current()->PostTask(FROM_HERE, m_runnableFactory.NewRunnableMethod(&WebRequest::startReading));
        }
    }
}

void WebRequest::handleInterceptedURL()
{
    m_loadState = Response;

    const std::string& mime = m_interceptResponse->mimeType();
    // Get the MIME type from the URL. "text/html" is a last resort, hopefully overridden.
    std::string mimeType("text/html");
    if (mime == "") {
        // Gmail appends the MIME to the end of the URL, with a ? separator.
        size_t mimeTypeIndex = m_url.find_last_of('?');
        if (mimeTypeIndex != std::string::npos) {
            mimeType.assign(m_url.begin() + mimeTypeIndex + 1, m_url.end());
        } else {
            // Get the MIME type from the file extension, if any.
            FilePath path(m_url);
            net::GetMimeTypeFromFile(path, &mimeType);
        }
    } else {
        // Set from the intercept response.
        mimeType = mime;
    }


    OwnPtr<WebResponse> webResponse(new WebResponse(m_url, mimeType, 0, m_interceptResponse->encoding(), m_interceptResponse->status()));
    m_urlLoader->maybeCallOnMainThread(NewRunnableMethod(
            m_urlLoader.get(), &WebUrlLoaderClient::didReceiveResponse, webResponse.release()));

    do {
        // data is deleted in WebUrlLoaderClient::didReceiveAndroidFileData
        // data is sent to the webcore thread
        OwnPtr<std::vector<char> > data(new std::vector<char>);
        data->reserve(kInitialReadBufSize);

        // Read returns false on error and size of 0 on eof.
        if (!m_interceptResponse->readStream(data.get()) || data->size() == 0)
            break;

        m_loadState = GotData;
        m_urlLoader->maybeCallOnMainThread(NewRunnableMethod(
                m_urlLoader.get(), &WebUrlLoaderClient::didReceiveAndroidFileData, data.release()));
    } while (true);

    finish(m_interceptResponse->status() == 200);
}

void WebRequest::handleDataURL(GURL url)
{
    OwnPtr<std::string> data(new std::string);
    std::string mimeType;
    std::string charset;

    if (net::DataURL::Parse(url, &mimeType, &charset, data.get())) {
        // PopulateURLResponse from chrome implementation
        // weburlloader_impl.cc
        m_loadState = Response;
        OwnPtr<WebResponse> webResponse(new WebResponse(url.spec(), mimeType, data->size(), charset, 200));
        m_urlLoader->maybeCallOnMainThread(NewRunnableMethod(
                m_urlLoader.get(), &WebUrlLoaderClient::didReceiveResponse, webResponse.release()));

        if (!data->empty()) {
            m_loadState = GotData;
            m_urlLoader->maybeCallOnMainThread(NewRunnableMethod(
                    m_urlLoader.get(), &WebUrlLoaderClient::didReceiveDataUrl, data.release()));
        }
    } else {
        // handle the failed case
    }

    finish(true);
}

void WebRequest::handleBrowserURL(GURL url)
{
    std::string data("data:text/html;charset=utf-8,");
    if (url.spec() == "browser:incognito") {
        AssetManager* assetManager = globalAssetManager();
        Asset* asset = assetManager->open("webkit/incognito_mode_start_page.html", Asset::ACCESS_BUFFER);
        if (asset) {
            data.append((const char*)asset->getBuffer(false), asset->getLength());
            delete asset;
        }
    }
    GURL dataURL(data.c_str());
    handleDataURL(dataURL);
}

// Called upon a server-initiated redirect.  The delegate may call the
// request's Cancel method to prevent the redirect from being followed.
// Since there may be multiple chained redirects, there may also be more
// than one redirect call.
//
// When this function is called, the request will still contain the
// original URL, the destination of the redirect is provided in 'new_url'.
// If the delegate does not cancel the request and |*defer_redirect| is
// false, then the redirect will be followed, and the request's URL will be
// changed to the new URL.  Otherwise if the delegate does not cancel the
// request and |*defer_redirect| is true, then the redirect will be
// followed once FollowDeferredRedirect is called on the URLRequest.
//
// The caller must set |*defer_redirect| to false, so that delegates do not
// need to set it if they are happy with the default behavior of not
// deferring redirect.
void WebRequest::OnReceivedRedirect(URLRequest* newRequest, const GURL& newUrl, bool* deferRedirect)
{
    ASSERT(m_loadState < Response, "Redirect after receiving response");
    ASSERT(newRequest && newRequest->status().is_success(), "Invalid redirect");

    OwnPtr<WebResponse> webResponse(new WebResponse(newRequest));
    webResponse->setUrl(newUrl.spec());
    m_urlLoader->maybeCallOnMainThread(NewRunnableMethod(
            m_urlLoader.get(), &WebUrlLoaderClient::willSendRequest, webResponse.release()));

    // Defer the redirect until followDeferredRedirect() is called.
    *deferRedirect = true;
}

// Called when we receive an authentication failure.  The delegate should
// call request->SetAuth() with the user's credentials once it obtains them,
// or request->CancelAuth() to cancel the login and display the error page.
// When it does so, the request will be reissued, restarting the sequence
// of On* callbacks.
void WebRequest::OnAuthRequired(URLRequest* request, net::AuthChallengeInfo* authInfo)
{
    ASSERT(m_loadState == Started, "OnAuthRequired called on a WebRequest not in STARTED state (state=%d)", m_loadState);

    scoped_refptr<net::AuthChallengeInfo> authInfoPtr(authInfo);
    bool firstTime = (m_authRequestCount == 0);
    ++m_authRequestCount;

    m_urlLoader->maybeCallOnMainThread(NewRunnableMethod(
            m_urlLoader.get(), &WebUrlLoaderClient::authRequired, authInfoPtr, firstTime));
}

// Called when we received an SSL certificate error. The delegate will provide
// the user the options to proceed, cancel, or view certificates.
void WebRequest::OnSSLCertificateError(URLRequest* request, int cert_error, net::X509Certificate* cert)
{
    m_urlLoader->maybeCallOnMainThread(NewRunnableMethod(
            m_urlLoader.get(), &WebUrlLoaderClient::reportSslCertError, cert_error, cert));
}

// After calling Start(), the delegate will receive an OnResponseStarted
// callback when the request has completed.  If an error occurred, the
// request->status() will be set.  On success, all redirects have been
// followed and the final response is beginning to arrive.  At this point,
// meta data about the response is available, including for example HTTP
// response headers if this is a request for a HTTP resource.
void WebRequest::OnResponseStarted(URLRequest* request)
{
    ASSERT(m_loadState == Started, "Got response after receiving response");

    m_loadState = Response;
    if (request && request->status().is_success()) {
        OwnPtr<WebResponse> webResponse(new WebResponse(request));
        m_urlLoader->maybeCallOnMainThread(NewRunnableMethod(
                m_urlLoader.get(), &WebUrlLoaderClient::didReceiveResponse, webResponse.release()));

        // Start reading the response
        startReading();
    } else {
        finish(false);
    }
}

void WebRequest::setAuth(const string16& username, const string16& password)
{
    ASSERT(m_loadState == Started, "setAuth called on a WebRequest not in STARTED state (state=%d)", m_loadState);

    m_request->SetAuth(username, password);
}

void WebRequest::cancelAuth()
{
    ASSERT(m_loadState == Started, "cancelAuth called on a WebRequest not in STARTED state (state=%d)", m_loadState);

    m_request->CancelAuth();
}

void WebRequest::followDeferredRedirect()
{
    ASSERT(m_loadState < Response, "Redirect after receiving response");

    m_request->FollowDeferredRedirect();
}

void WebRequest::proceedSslCertError()
{
    m_request->ContinueDespiteLastError();
}

void WebRequest::cancelSslCertError(int cert_error)
{
    m_request->SimulateError(cert_error);
}

void WebRequest::startReading()
{
    ASSERT(m_networkBuffer == 0, "startReading called with a nonzero buffer");
    ASSERT(m_isPaused == 0, "startReading called in paused state");
    ASSERT(m_loadState == Response || m_loadState == GotData, "StartReading in state other than RESPONSE and GOTDATA");
    if (m_loadState > GotData) // We have been cancelled between reads
        return;

    if (m_wantToPause) {
        m_isPaused = true;
        return;
    }

    int bytesRead = 0;

    if (!read(&bytesRead)) {
        if (m_request && m_request->status().is_io_pending())
            return; // Wait for OnReadCompleted()
        return finish(false);
    }

    // bytesRead == 0 indicates finished
    if (!bytesRead)
        return finish(true);

    m_loadState = GotData;
    // Read ok, forward buffer to webcore
    m_urlLoader->maybeCallOnMainThread(NewRunnableMethod(m_urlLoader.get(), &WebUrlLoaderClient::didReceiveData, m_networkBuffer, bytesRead));
    m_networkBuffer = 0;
    MessageLoop::current()->PostTask(FROM_HERE, m_runnableFactory.NewRunnableMethod(&WebRequest::startReading));
}

bool WebRequest::read(int* bytesRead)
{
    ASSERT(m_loadState == Response || m_loadState == GotData, "read in state other than RESPONSE and GOTDATA");
    ASSERT(m_networkBuffer == 0, "Read called with a nonzero buffer");

    // TODO: when asserts work, check that the buffer is 0 here
    m_networkBuffer = new net::IOBuffer(kInitialReadBufSize);
    return m_request->Read(m_networkBuffer, kInitialReadBufSize, bytesRead);
}

// This is called when there is data available

// Called when the a Read of the response body is completed after an
// IO_PENDING status from a Read() call.
// The data read is filled into the buffer which the caller passed
// to Read() previously.
//
// If an error occurred, request->status() will contain the error,
// and bytes read will be -1.
void WebRequest::OnReadCompleted(URLRequest* request, int bytesRead)
{
    ASSERT(m_loadState == Response || m_loadState == GotData, "OnReadCompleted in state other than RESPONSE and GOTDATA");

    if (request->status().is_success()) {
        m_loadState = GotData;
        m_urlLoader->maybeCallOnMainThread(NewRunnableMethod(
                m_urlLoader.get(), &WebUrlLoaderClient::didReceiveData, m_networkBuffer, bytesRead));
        m_networkBuffer = 0;

        // Get the rest of the data
        startReading();
    } else {
        finish(false);
    }
}

} // namespace android
