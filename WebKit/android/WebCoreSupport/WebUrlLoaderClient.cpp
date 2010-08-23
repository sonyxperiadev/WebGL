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
#include "WebUrlLoaderClient.h"

#include "OwnPtr.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceResponse.h"
#include "WebCoreFrameBridge.h"
#include "WebRequest.h"
#include "WebResourceRequest.h"

namespace android {

LoaderData::~LoaderData()
{
    if (buffer)
        buffer->Release();
}

base::Thread* WebUrlLoaderClient::ioThread()
{
    static base::Thread* networkThread = 0;
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

Lock* WebUrlLoaderClient::syncLock() {
    static Lock s_syncLock;
    return &s_syncLock;
}

ConditionVariable* WebUrlLoaderClient::syncCondition() {
    static ConditionVariable s_syncCondition(syncLock());
    return &s_syncCondition;
}

WebUrlLoaderClient::~WebUrlLoaderClient()
{
    base::Thread* thread = ioThread();
    if (thread)
        thread->message_loop()->ReleaseSoon(FROM_HERE, m_request);
}

bool WebUrlLoaderClient::isActive() const
{
    if (m_cancelling)
        return false;
    if (!m_resourceHandle->client())
        return false;

    return true;
}

WebUrlLoaderClient::WebUrlLoaderClient(WebFrame* webFrame, WebCore::ResourceHandle* resourceHandle, const WebCore::ResourceRequest& resourceRequest)
    : m_webFrame(webFrame)
    , m_resourceHandle(resourceHandle)
    , m_cancelling(false)
    , m_sync(false)
    , m_finished(false)
{
    WebResourceRequest webResourceRequest(resourceRequest);
    if (webResourceRequest.isAndroidUrl()) {
        int inputStream = webFrame->inputStreamForAndroidResource(webResourceRequest.url().c_str(), webResourceRequest.androidFileType());
        m_request = new WebRequest(this, webResourceRequest, inputStream);
        m_request->AddRef(); // Matched by ReleaseSoon in destructor
        return;
    }

    m_request = new WebRequest(this, webResourceRequest);
    m_request->AddRef(); // Matched by ReleaseSoon in destructor

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
                    int size  = static_cast<int>(element.m_data.size());
                    // TODO: do we have to make a copy of this data?
                    base::Thread* thread = ioThread();
                    if (thread)
                        thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request, &WebRequest::AppendBytesToUpload, element.m_data.data(), size));
                }
                break;
#if ENABLE(BLOB)
            case FormDataElement::encodedFile:
                if (element.m_fileLength == -1)
                    continue; // TODO: Not supporting directories yet
                else {
                    // TODO: Add fileuploads after Google log-in is fixed.
                    // Chrome code is here: webkit/glue/weburlloader_impl.cc:391
                }
                break;
#endif
            default:
                // TODO: Add a warning/DCHECK/assert here, should never happen
                break;
            }
        }
    }
}

bool WebUrlLoaderClient::start(bool sync, bool isPrivateBrowsing)
{
    base::Thread* thread = ioThread();
    if (!thread) {
        return false;
    }

    m_sync = sync;
    if (m_sync) {
        AutoLock autoLock(*syncLock());
        thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request, &WebRequest::start, isPrivateBrowsing));

        // Run callbacks until the queue is exhausted and m_finished is true.
        while(!m_finished) {
            while (!m_queue.empty()) {
                Callback& callback = m_queue.front();
                CallbackFunction* function = callback.a;
                void* context = callback.b;
                (*function)(context);
                m_queue.pop_front();
            }
            if (m_queue.empty() && !m_finished) {
                syncCondition()->Wait();
            }
        }

        // This may be the last reference to us, so we may be deleted now.
        // Don't access any more member variables after releasing this reference.
        m_resourceHandle = 0;
    } else {
        // Asynchronous start.
        thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request, &WebRequest::start, isPrivateBrowsing));
    }
    return true;
}

void WebUrlLoaderClient::cancel()
{
    m_cancelling = true;

    base::Thread* thread = ioThread();
    if (thread)
        thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request, &WebRequest::cancel));
}

void WebUrlLoaderClient::setAuth(const std::string& username, const std::string& password)
{
    base::Thread* thread = ioThread();
    if (!thread) {
        return;
    }
    std::wstring wUsername = base::SysUTF8ToWide(username);
    std::wstring wPassword = base::SysUTF8ToWide(password);
    thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request, &WebRequest::setAuth, wUsername, wPassword));
}

void WebUrlLoaderClient::cancelAuth()
{
    base::Thread* thread = ioThread();
    if (!thread) {
        return;
    }
    thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request, &WebRequest::cancelAuth));
}

void WebUrlLoaderClient::finish()
{
    m_finished = true;
    if (!m_sync) {
        // This is the last reference to us, so we will be deleted now.
        // We only release the reference here if start() was called asynchronously!
        m_resourceHandle = 0;
    }
}

// This is called from the IO thread, and dispatches the callback to the main thread.
void WebUrlLoaderClient::maybeCallOnMainThread(CallbackFunction* function, void* context)
{
    if (m_sync) {
        AutoLock autoLock(*syncLock());
        if (m_queue.empty()) {
            syncCondition()->Broadcast();
        }
        m_queue.push_back(Callback(function, context));
    } else {
        // Let WebKit handle it.
        callOnMainThread(function, context);
    }
}

// Response methods
// static - on main thread
void WebUrlLoaderClient::didReceiveResponse(void* data)
{
    OwnPtr<LoaderData> loaderData(static_cast<LoaderData*>(data));
    const WebUrlLoaderClient* loader = loaderData->loader;
    WebResponse webResponse = loaderData->webResponse;

    if (!loader->isActive())
        return;

    loader->m_resourceHandle->client()->didReceiveResponse(loader->m_resourceHandle.get(), webResponse.createResourceResponse());
}

// static - on main thread
void WebUrlLoaderClient::didReceiveData(void* data)
{
    OwnPtr<LoaderData> loaderData(static_cast<LoaderData*>(data));
    const WebUrlLoaderClient* loader = loaderData->loader;
    net::IOBuffer* buf = loaderData->buffer;

    if (!loader->isActive())
        return;

    // didReceiveData will take a copy of the data
    if (loader->m_resourceHandle && loader->m_resourceHandle->client())
        loader->m_resourceHandle->client()->didReceiveData(loader->m_resourceHandle.get(), buf->data(), loaderData->size, loaderData->size);
}

// static - on main thread
// For data url's
void WebUrlLoaderClient::didReceiveDataUrl(void* data)
{
    OwnPtr<LoaderData> loaderData(static_cast<LoaderData*>(data));
    const WebUrlLoaderClient* loader = loaderData->loader;

    if (!loader->isActive())
        return;

    std::string* str = loaderData->string.get();
    // didReceiveData will take a copy of the data
    loader->m_resourceHandle->client()->didReceiveData(loader->m_resourceHandle.get(), str->data(), str->size(), str->size());
}

// static - on main thread
// For special android files
void WebUrlLoaderClient::didReceiveAndroidFileData(void* data)
{
    OwnPtr<LoaderData> loaderData(static_cast<LoaderData*>(data));
    const WebUrlLoaderClient* loader = loaderData->loader;

    if (!loader->isActive())
        return;

    // didReceiveData will take a copy of the data
    loader->m_resourceHandle->client()->didReceiveData(loader->m_resourceHandle.get(), &(loaderData->vector->front()), loaderData->size, loaderData->size);
}

// static - on main thread
void WebUrlLoaderClient::didFail(void* data)
{
    OwnPtr<LoaderData> loaderData(static_cast<LoaderData*>(data));
    WebUrlLoaderClient* loader = loaderData->loader;

    if (loader->isActive())
        loader->m_resourceHandle->client()->didFail(loader->m_resourceHandle.get(), loaderData->webResponse.createResourceError());

    // Always finish a request, if not it will leak
    loader->finish();
}

// static - on main thread
void WebUrlLoaderClient::willSendRequest(void* data)
{
    OwnPtr<LoaderData> loaderData(static_cast<LoaderData*>(data));
    const WebUrlLoaderClient* loader = loaderData->loader;

    if (!loader->isActive())
        return;

    WebResponse webResponse = loaderData->webResponse;
    OwnPtr<WebCore::ResourceRequest> resourceRequest(new WebCore::ResourceRequest(webResponse.url()));
    loader->m_resourceHandle->client()->willSendRequest(loader->m_resourceHandle.get(), *resourceRequest, webResponse.createResourceResponse());
}

// static - on main thread
void WebUrlLoaderClient::didFinishLoading(void* data)
{
    OwnPtr<LoaderData> loaderData(static_cast<LoaderData*>(data));
    WebUrlLoaderClient* loader = loaderData->loader;

    if (loader->isActive())
        loader->m_resourceHandle->client()->didFinishLoading(loader->m_resourceHandle.get());

    // Always finish a request, if not it will leak
    loader->finish();
}

// static - on main thread
void WebUrlLoaderClient::authRequired(void* data)
{
    OwnPtr<LoaderData> loaderData(static_cast<LoaderData*>(data));
    WebUrlLoaderClient* loader = loaderData->loader;

    if (!loader->isActive()) {
        return;
    }

    std::string host = base::SysWideToUTF8(loaderData->authChallengeInfo->host_and_port);
    std::string realm = base::SysWideToUTF8(loaderData->authChallengeInfo->realm);

    // TODO: Not clear whose responsibility it is to cache credentials. There's nothing
    // in AuthChallengeInfo that seems suitable, so for safety we'll tell the UI *not*
    // to use cached credentials. We may need to track this ourselves (pass "true" on
    // the first call, then "false" for a second call if the credentials are rejected).
    bool useCachedCredentials = false;

    loader->m_webFrame->didReceiveAuthenticationChallenge(loader, host, realm, useCachedCredentials);
}

} // namespace android
