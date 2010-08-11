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

#include "MainThread.h"
#include "WebRequestContext.h"
#include "WebResourceRequest.h"

#include <net/base/data_url.h>
#include <net/base/io_buffer.h>
#include <net/http/http_response_headers.h>
#include <net/url_request/url_request.h>
#include <string>

// TODO:
// - Get gmail log in to work
// - Finish the file upload. Testcase is mobile buzz
// - Handle fails better
// - Check the string conversion work for more than the general case
// - Add network throttle needed by Android plugins

namespace android {

namespace {
    const int kInitialReadBufSize = 32768;
}

WebRequest::WebRequest(WebUrlLoaderClient* loader, WebResourceRequest webResourceRequest) : m_urlLoader(loader), m_request(0)
{
    GURL gurl(webResourceRequest.url());
    m_request = new URLRequest(gurl, this);

    m_request->SetExtraRequestHeaders(webResourceRequest.requestHeaders());
    m_request->set_referrer(webResourceRequest.referrer());
    m_request->set_method(webResourceRequest.method());
}

WebRequest::~WebRequest()
{
}

void WebRequest::finish(bool success)
{
    if (success) {
        LoaderData* loaderData = new LoaderData(m_urlLoader);
        callOnMainThread(WebUrlLoaderClient::didFinishLoading, loaderData);
    } else {
        WebResponse webResponse(m_request.get());
        LoaderData* loaderData = new LoaderData(m_urlLoader, webResponse);
        callOnMainThread(WebUrlLoaderClient::didFail, loaderData);
    }
    m_networkBuffer = 0;
    m_request = 0;
}

void WebRequest::AppendBytesToUpload(const char* bytes, int bytesLen)
{
    m_request->AppendBytesToUpload(bytes, bytesLen);
}

void WebRequest::start()
{
    // Handle data urls before we send it off to the http stack
    if (m_request->url().SchemeIs("data"))
        return handleDataURL(m_request->url());

    m_request->set_context(WebRequestContext::GetAndroidContext());
    m_request->Start();
}

void WebRequest::cancel()
{
    if (m_request)
        m_request->Cancel();
    finish(true);
}

void WebRequest::handleDataURL(GURL url)
{
    OwnPtr<std::string*> data(new std::string);
    std::string mimeType;
    std::string charset;

    if (net::DataURL::Parse(url, &mimeType, &charset, data.get())) {
        // PopulateURLResponse from chrome implementation
        // weburlloader_impl.cc
        WebResponse webResponse(url.spec(), mimeType, data->size(), charset, 200);
        LoaderData* loaderResponse = new LoaderData(m_urlLoader, webResponse);
        callOnMainThread(WebUrlLoaderClient::didReceiveResponse, loaderResponse);

        if (!data->empty()) {
            LoaderData* loaderData = new LoaderData(m_urlLoader, data.leakPtr());
            callOnMainThread(WebUrlLoaderClient::didReceiveDataUrl, loaderData);
        }
    } else {
        // handle the failed case
    }

    finish(true);
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
    if (newRequest && newRequest->status().is_success()) {
        WebResponse webResponse(newRequest);
        webResponse.setUrl(newUrl.spec());
        LoaderData* ld = new LoaderData(m_urlLoader, webResponse);
        callOnMainThread(WebUrlLoaderClient::willSendRequest, ld);
    } else {
        // why would this happen? And what to do?
    }

    // Here we should check if the url we get back from webkit is the same
    // as newUrl, but since we are on a different thread that is not
    // possible. Look into later.
    return;
}

// Called when we receive an authentication failure.  The delegate should
// call request->SetAuth() with the user's credentials once it obtains them,
// or request->CancelAuth() to cancel the login and display the error page.
// When it does so, the request will be reissued, restarting the sequence
// of On* callbacks.
void WebRequest::OnAuthRequired(URLRequest* request, net::AuthChallengeInfo* authInfo)
{
    // TODO: This is the default action, have to implement getting the
    // username and password from webkit
    request->CancelAuth();
}

// After calling Start(), the delegate will receive an OnResponseStarted
// callback when the request has completed.  If an error occurred, the
// request->status() will be set.  On success, all redirects have been
// followed and the final response is beginning to arrive.  At this point,
// meta data about the response is available, including for example HTTP
// response headers if this is a request for a HTTP resource.
void WebRequest::OnResponseStarted(URLRequest* request)
{
    if (request && request->status().is_success()) {
        WebResponse webResponse(request);
        LoaderData* loaderData = new LoaderData(m_urlLoader, webResponse);
        callOnMainThread(WebUrlLoaderClient::didReceiveResponse, loaderData);

        // Start reading the response
        startReading();
    } else {
        finish(false);
    }
}

void WebRequest::startReading()
{
    int bytesRead = 0;

    // chrome only does one read, and schedules the next on the same thread
    while (true) {
        if (read(&bytesRead)) {
            // bytesRead == 0 indicates finished
            if (!bytesRead) {
                finish(true);
                break;
            }

            // Read ok, forward buffer to webcore
            m_networkBuffer->AddRef();
            LoaderData* loaderData = new LoaderData(m_urlLoader, m_networkBuffer.get(), bytesRead);
            callOnMainThread(WebUrlLoaderClient::didReceiveData, loaderData);
            // m_networkBuffer->Release() on main thread
            m_networkBuffer = 0;
        } else if (m_request && m_request->status().is_io_pending()) {
            // got io_pending, so break and wait for read
            break;
        } else {
            // Error, stop and send back
            finish(false);
            break;
        }
    }
}

bool WebRequest::read(int* bytesRead)
{
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
    if (request->status().is_success()) {
        m_networkBuffer->AddRef();
        LoaderData* loaderData = new LoaderData(m_urlLoader, m_networkBuffer.get(), bytesRead);
        // m_networkBuffer->Release() on main thread
        callOnMainThread(WebUrlLoaderClient::didReceiveData, loaderData);
        m_networkBuffer = 0;

        // Get the rest of the data
        startReading();
    } else {
        finish(false);
    }
}

} // namespace android
