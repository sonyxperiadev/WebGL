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

#define LOG_TAG "WebUrlLoaderClient"

#include "config.h"
#include "WebUrlLoaderClient.h"

#include "ChromiumIncludes.h"
#include "OwnPtr.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceResponse.h"
#include "WebCoreFrameBridge.h"
#include "WebRequest.h"
#include "WebResourceRequest.h"

#include <wtf/text/CString.h>

using base::Lock;
using base::AutoLock;

namespace android {

base::Thread* WebUrlLoaderClient::ioThread()
{
    static base::Thread* networkThread = 0;
    static Lock networkThreadLock;

    // Multiple threads appear to access the ioThread so we must ensure the
    // critical section ordering.
    AutoLock lock(networkThreadLock);

    if (!networkThread)
        networkThread = new base::Thread("network");

    if (!networkThread)
        return 0;

    if (networkThread->IsRunning())
        return networkThread;

    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    if (!networkThread->StartWithOptions(options)) {
        delete networkThread;
        networkThread = 0;
    }

    return networkThread;
}

base::Lock* WebUrlLoaderClient::syncLock() {
    static Lock s_syncLock;
    return &s_syncLock;
}

base::ConditionVariable* WebUrlLoaderClient::syncCondition() {
    static base::ConditionVariable s_syncCondition(syncLock());
    return &s_syncCondition;
}

WebUrlLoaderClient::~WebUrlLoaderClient()
{
}

bool WebUrlLoaderClient::isActive() const
{
    if (m_cancelling)
        return false;
    if (!m_resourceHandle)
        return false;
    if (!m_resourceHandle->client())
        return false;
    if (m_finished)
        return false;

    return true;
}

WebUrlLoaderClient::WebUrlLoaderClient(WebFrame* webFrame, WebCore::ResourceHandle* resourceHandle, const WebCore::ResourceRequest& resourceRequest)
    : m_webFrame(webFrame)
    , m_resourceHandle(resourceHandle)
    , m_isMainResource(false)
    , m_isMainFrame(false)
    , m_isCertMimeType(false)
    , m_cancelling(false)
    , m_sync(false)
    , m_finished(false)
{
    bool block = webFrame->blockNetworkLoads() && (resourceRequest.url().protocolIs("http") || resourceRequest.url().protocolIs("https"));
    WebResourceRequest webResourceRequest(resourceRequest, block);
    UrlInterceptResponse* intercept = webFrame->shouldInterceptRequest(resourceRequest.url().string());
    if (intercept) {
        m_request = new WebRequest(this, webResourceRequest, intercept);
        return;
    }

    m_request = new WebRequest(this, webResourceRequest);

    // Set uploads before start is called on the request
    if (resourceRequest.httpBody() && !(webResourceRequest.method() == "GET" || webResourceRequest.method() == "HEAD")) {
        Vector<FormDataElement>::iterator iter;
        Vector<FormDataElement> elements = resourceRequest.httpBody()->elements();
        for (iter = elements.begin(); iter != elements.end(); iter++) {
            FormDataElement element = *iter;

            switch (element.m_type) {
            case FormDataElement::data:
                if (!element.m_data.isEmpty()) {
                    // WebKit sometimes gives up empty data to append. These aren't
                    // necessary so we just optimize those out here.
                    base::Thread* thread = ioThread();
                    if (thread) {
                        Vector<char>* data = new Vector<char>(element.m_data);
                        thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request.get(), &WebRequest::appendBytesToUpload, data));
                    }
                }
                break;
            case FormDataElement::encodedFile:
                {
                    // Chromium check if it is a directory by checking
                    // element.m_fileLength, that doesn't work in Android
                    std::string filename = element.m_filename.utf8().data();
                    if (filename.size()) {
                        // Change from a url string to a filename
                        if (filename.find("file://") == 0) // Found at pos 0
                            filename.erase(0, 7);
                        base::Thread* thread = ioThread();
                        if (thread)
                            thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request.get(), &WebRequest::appendFileToUpload, filename));
                    }
                }
                break;
#if ENABLE(BLOB)
            case FormDataElement::encodedBlob:
                LOG_ASSERT(false, "Unexpected use of FormDataElement::encodedBlob");
                break;
#endif // ENABLE(BLOB)
            default:
                LOG_ASSERT(false, "Unexpected default case in WebUrlLoaderClient.cpp");
                break;
            }
        }
    }
}

bool WebUrlLoaderClient::start(bool isMainResource, bool isMainFrame, bool sync, WebRequestContext* context)
{
    base::Thread* thread = ioThread();
    if (!thread) {
        return false;
    }

    m_isMainResource = isMainResource;
    m_isMainFrame = isMainFrame;
    m_sync = sync;
    if (m_sync) {
        AutoLock autoLock(*syncLock());
        m_request->setSync(sync);
        m_request->setRequestContext(context);
        thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request.get(), &WebRequest::start));

        // Run callbacks until the queue is exhausted and m_finished is true.
        // Sometimes, a sync load can wait forever and lock up the WebCore thread,
        // here we use TimedWait() with multiple tries to avoid locking.
        const int kMaxNumTimeout = 3;
        const int kCallbackWaitingTime = 10;
        int num_timeout = 0;
        while(!m_finished) {
            while (!m_queue.empty()) {
                OwnPtr<Task> task(m_queue.front());
                m_queue.pop_front();
                task->Run();
            }
            if (m_finished) break;

            syncCondition()->TimedWait(base::TimeDelta::FromSeconds(kCallbackWaitingTime));
            if (m_queue.empty()) {
                LOGE("Synchronous request timed out after %d seconds for the %dth try, URL: %s",
                     kCallbackWaitingTime, num_timeout, m_request->getUrl().c_str());
                num_timeout++;
                if (num_timeout >= kMaxNumTimeout) {
                    cancel();
                    m_resourceHandle = 0;
                    return false;
                }
            }
        }

        // This may be the last reference to us, so we may be deleted now.
        // Don't access any more member variables after releasing this reference.
        m_resourceHandle = 0;
    } else {
        // Asynchronous start.
        // Important to set this before the thread starts so it has a reference and can't be deleted
        // before the task starts running on the IO thread.
        m_request->setRequestContext(context);
        thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request.get(), &WebRequest::start));
    }
    return true;
}

namespace {
// Check if the mime type is for certificate installation.
// The items must be consistent with the sCertificateTypeMap
// in frameworks/base/core/java/android/webkit/CertTool.java.
bool isMimeTypeForCert(const std::string& mimeType)
{
    static std::hash_set<std::string> sCertificateTypeSet;
    if (sCertificateTypeSet.empty()) {
        sCertificateTypeSet.insert("application/x-x509-ca-cert");
        sCertificateTypeSet.insert("application/x-x509-user-cert");
        sCertificateTypeSet.insert("application/x-pkcs12");
    }
    return sCertificateTypeSet.find(mimeType) != sCertificateTypeSet.end();
}
}

void WebUrlLoaderClient::downloadFile()
{
    if (m_response) {
        std::string contentDisposition;
        m_response->getHeader("content-disposition", &contentDisposition);
        m_webFrame->downloadStart(m_response->getUrl(), m_request->getUserAgent(), contentDisposition, m_response->getMimeType(), m_response->getExpectedSize());

        m_isCertMimeType = isMimeTypeForCert(m_response->getMimeType());
        // Currently, only certificate mime type needs to receive the data.
        // Other mime type, e.g. wav, will send the url to other application
        // which will load the data by url.
        if (!m_isCertMimeType)
            cancel();
    } else {
        LOGE("Unexpected call to downloadFile() before didReceiveResponse(). URL: %s", m_request->getUrl().c_str());
        // TODO: Turn off asserts crashing before release
        // http://b/issue?id=2951985
        CRASH();
    }
}

void WebUrlLoaderClient::cancel()
{
    if (!isActive())
        return;

    m_cancelling = true;

    base::Thread* thread = ioThread();
    if (thread)
        thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request.get(), &WebRequest::cancel));
}

void WebUrlLoaderClient::pauseLoad(bool pause)
{
    if (!isActive())
        return;

    base::Thread* thread = ioThread();
    if (thread)
        thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request.get(), &WebRequest::pauseLoad, pause));
}

void WebUrlLoaderClient::setAuth(const std::string& username, const std::string& password)
{
    if (!isActive())
        return;

    base::Thread* thread = ioThread();
    if (!thread) {
        return;
    }
    string16 username16 = ASCIIToUTF16(username);
    string16 password16 = ASCIIToUTF16(password);
    thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request.get(), &WebRequest::setAuth, username16, password16));
}

void WebUrlLoaderClient::cancelAuth()
{
    if (!isActive())
        return;

    base::Thread* thread = ioThread();
    if (!thread) {
        return;
    }
    thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request.get(), &WebRequest::cancelAuth));
}

void WebUrlLoaderClient::proceedSslCertError()
{
    base::Thread* thread = ioThread();
    if (isActive() && thread)
        thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request.get(), &WebRequest::proceedSslCertError));
    this->Release();
}

void WebUrlLoaderClient::cancelSslCertError(int cert_error)
{
    base::Thread* thread = ioThread();
    if (isActive() && thread)
        thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request.get(), &WebRequest::cancelSslCertError, cert_error));
    this->Release();
}

void WebUrlLoaderClient::finish()
{
    m_finished = true;
    if (!m_sync) {
        // This is the last reference to us, so we will be deleted now.
        // We only release the reference here if start() was called asynchronously!
        m_resourceHandle = 0;
    }
    m_request = 0;
}

namespace {
// Trampoline to wrap a Chromium Task* in a WebKit-style static function + void*.
static void RunTask(void* v) {
    OwnPtr<Task> task(static_cast<Task*>(v));
    task->Run();
}
}

// This is called from the IO thread, and dispatches the callback to the main thread.
void WebUrlLoaderClient::maybeCallOnMainThread(Task* task)
{
    if (m_sync) {
        AutoLock autoLock(*syncLock());
        if (m_queue.empty()) {
            syncCondition()->Broadcast();
        }
        m_queue.push_back(task);
    } else {
        // Let WebKit handle it.
        callOnMainThread(RunTask, task);
    }
}

// Response methods
void WebUrlLoaderClient::didReceiveResponse(PassOwnPtr<WebResponse> webResponse)
{
    if (!isActive())
        return;

    m_response = webResponse;
    m_resourceHandle->client()->didReceiveResponse(m_resourceHandle.get(), m_response->createResourceResponse());

    // Set the main page's certificate to WebView.
    if (m_isMainResource && m_isMainFrame) {
        const net::SSLInfo& ssl_info = m_response->getSslInfo();
        if (ssl_info.is_valid()) {
            std::vector<std::string> chain_bytes;
            ssl_info.cert->GetChainDEREncodedBytes(&chain_bytes);
            m_webFrame->setCertificate(chain_bytes[0]);
        }

        // Look for X-Auto-Login on the main resource to log in the user.
        std::string login;
        if (m_response->getHeader("x-auto-login", &login))
            m_webFrame->autoLogin(login);
    }
}

void WebUrlLoaderClient::didReceiveData(scoped_refptr<net::IOBuffer> buf, int size)
{
    if (m_isMainResource && m_isCertMimeType) {
        m_webFrame->didReceiveData(buf->data(), size);
    }

    if (!isActive() || !size)
        return;

    // didReceiveData will take a copy of the data
    if (m_resourceHandle && m_resourceHandle->client())
        m_resourceHandle->client()->didReceiveData(m_resourceHandle.get(), buf->data(), size, size);
}

// For data url's
void WebUrlLoaderClient::didReceiveDataUrl(PassOwnPtr<std::string> str)
{
    if (!isActive() || !str->size())
        return;

    // didReceiveData will take a copy of the data
    m_resourceHandle->client()->didReceiveData(m_resourceHandle.get(), str->data(), str->size(), str->size());
}

// For special android files
void WebUrlLoaderClient::didReceiveAndroidFileData(PassOwnPtr<std::vector<char> > vector)
{
    if (!isActive() || !vector->size())
        return;

    // didReceiveData will take a copy of the data
    m_resourceHandle->client()->didReceiveData(m_resourceHandle.get(), vector->begin(), vector->size(), vector->size());
}

void WebUrlLoaderClient::didFail(PassOwnPtr<WebResponse> webResponse)
{
    if (isActive())
        m_resourceHandle->client()->didFail(m_resourceHandle.get(), webResponse->createResourceError());

    // Always finish a request, if not it will leak
    finish();
}

void WebUrlLoaderClient::willSendRequest(PassOwnPtr<WebResponse> webResponse)
{
    if (!isActive())
        return;

    KURL url = webResponse->createKurl();
    OwnPtr<WebCore::ResourceRequest> resourceRequest(new WebCore::ResourceRequest(url));
    m_resourceHandle->client()->willSendRequest(m_resourceHandle.get(), *resourceRequest, webResponse->createResourceResponse());

    // WebKit may have killed the request.
    if (!isActive())
        return;

    // Like Chrome, we only follow the redirect if WebKit left the URL unmodified.
    if (url == resourceRequest->url()) {
        ioThread()->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request.get(), &WebRequest::followDeferredRedirect));
    } else {
        cancel();
    }
}

void WebUrlLoaderClient::didFinishLoading()
{
    if (isActive())
        m_resourceHandle->client()->didFinishLoading(m_resourceHandle.get(), 0);

    if (m_isMainResource && m_isCertMimeType) {
        m_webFrame->didFinishLoading();
    }

    // Always finish a request, if not it will leak
    finish();
}

void WebUrlLoaderClient::authRequired(scoped_refptr<net::AuthChallengeInfo> authChallengeInfo, bool firstTime, bool suppressDialog)
{
    if (!isActive())
        return;

    std::string host = base::SysWideToUTF8(authChallengeInfo->host_and_port);
    std::string realm = base::SysWideToUTF8(authChallengeInfo->realm);

    m_webFrame->didReceiveAuthenticationChallenge(this, host, realm, firstTime, suppressDialog);
}

void WebUrlLoaderClient::reportSslCertError(int cert_error, net::X509Certificate* cert)
{
    if (!isActive())
        return;

    std::vector<std::string> chain_bytes;
    cert->GetChainDEREncodedBytes(&chain_bytes);
    this->AddRef();
    m_webFrame->reportSslCertError(this, cert_error, chain_bytes[0], m_request->getUrl());
}

void WebUrlLoaderClient::sslClientCert(EVP_PKEY* pkey, net::X509Certificate* chain)
{
    base::Thread* thread = ioThread();
    scoped_refptr<net::X509Certificate> scopedChain(chain);
    if (isActive() && thread)
        thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request.get(), &WebRequest::sslClientCert, pkey, scopedChain));
    this->Release();
}

void WebUrlLoaderClient::requestClientCert(net::SSLCertRequestInfo* cert_request_info)
{
    if (!isActive())
        return;

    std::string host_and_port = cert_request_info->host_and_port;
    this->AddRef();
    m_webFrame->requestClientCert(this, host_and_port);
}

} // namespace android
