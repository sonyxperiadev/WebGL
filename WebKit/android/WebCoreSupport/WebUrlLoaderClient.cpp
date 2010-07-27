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
#include "WebRequest.h"

#include "base/thread.h"
#include "net/base/io_buffer.h"

namespace android {

LoaderData::~LoaderData()
{
    if (m_buffer)
        m_buffer->Release();
}

std::vector<WebUrlLoaderClient*>* WebUrlLoaderClient::activeLoaders = new std::vector<WebUrlLoaderClient*>();

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

WebUrlLoaderClient::~WebUrlLoaderClient()
{
    base::Thread* thread = ioThread();
    if (thread)
        thread->message_loop()->ReleaseSoon(FROM_HERE, m_request);

    std::vector<WebUrlLoaderClient*>::iterator iter;
    for (iter = activeLoaders->begin(); iter != activeLoaders->end(); ++iter) {
        if (*iter == this) {
            activeLoaders->erase(iter);
            break;
        }
    }
}

bool WebUrlLoaderClient::isActive(const WebUrlLoaderClient* loader)
{
    if (!loader)
        return false;

    bool exists = false;
    std::vector<WebUrlLoaderClient*>::const_iterator iter;
    for (iter = activeLoaders->begin(); iter != activeLoaders->end(); ++iter)
        if (*iter == loader)
            exists = true;

    if (!exists)
        return false;
    if (loader->m_cancelling)
        return false;
    if (!loader->m_resourceHandle)
        return false;
    if (!loader->m_resourceHandle->client())
        return false;

    return true;
}

WebUrlLoaderClient::WebUrlLoaderClient(WebCore::ResourceHandle* resourceHandle, const WebCore::ResourceRequest& resourceRequest)
        : m_resourceHandle(resourceHandle), m_resourceRequest(resourceRequest), m_cancelling(false)
{
    m_request = new WebRequest(this, m_resourceRequest);
    m_request->AddRef(); // Matched by ReleaseSoon in destructor

    activeLoaders->push_back(this);
}

bool WebUrlLoaderClient::start(bool /*sync*/)
{
    base::Thread* thread = ioThread();
    if (thread) {
        thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request, &WebRequest::start));
        return true;
    }

    return false;
}

void WebUrlLoaderClient::cancel()
{
    m_cancelling = true;

    base::Thread* thread = ioThread();
    if (thread)
        thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(m_request, &WebRequest::cancel));
}

void WebUrlLoaderClient::finish()
{
    m_resourceHandle = 0;
}

// Response methods
// static - on main thread
void WebUrlLoaderClient::didReceiveResponse(void* data)
{
    OwnPtr<LoaderData> loaderData(static_cast<LoaderData*>(data));
    const WebUrlLoaderClient* loader = loaderData->m_loader;
    ResourceResponse* resourceResponse = loaderData->m_resourceResponse.get();

    if (!isActive(loader))
        return;

    loader->m_resourceHandle->client()->didReceiveResponse(loader->m_resourceHandle.get(), *resourceResponse);
}

// static - on main thread
void WebUrlLoaderClient::didReceiveData(void* data)
{
    OwnPtr<LoaderData> loaderData(static_cast<LoaderData*>(data));
    const WebUrlLoaderClient* loader = loaderData->m_loader;
    net::IOBuffer* buf = loaderData->m_buffer;

    if (!isActive(loader))
        return;

    if (loader->m_resourceHandle && loader->m_resourceHandle->client())
        loader->m_resourceHandle->client()->didReceiveData(loader->m_resourceHandle.get(), buf->data(), loaderData->m_size, loaderData->m_size);
}

// static - on main thread
// For data url's
void WebUrlLoaderClient::didReceiveDataUrl(void* data)
{
    OwnPtr<LoaderData> ld(static_cast<LoaderData*>(data));
    const WebUrlLoaderClient* loader = ld->m_loader;

    if (!isActive(loader))
        return;

    std::string* str = ld->m_string.get();
    loader->m_resourceHandle->client()->didReceiveData(loader->m_resourceHandle.get(), str->data(), str->size(), str->size());
}

// static - on main thread
void WebUrlLoaderClient::didFail(void* data)
{
}

// static - on main thread
void WebUrlLoaderClient::willSendRequest(void* data)
{
    OwnPtr<LoaderData> loaderData(static_cast<LoaderData*>(data));
    const WebUrlLoaderClient* loader = loaderData->m_loader;

    if (!isActive(loader))
        return;

    WebCore::ResourceResponse* resourceResponse = loaderData->m_resourceResponse.get();
    OwnPtr<WebCore::ResourceRequest> resourceRequest(new WebCore::ResourceRequest(resourceResponse->url()));
    loader->m_resourceHandle->client()->willSendRequest(loader->m_resourceHandle.get(), *resourceRequest, *resourceResponse);
}

// static - on main thread
void WebUrlLoaderClient::didFinishLoading(void* data)
{
    OwnPtr<LoaderData> loaderData(static_cast<LoaderData*>(data));
    WebUrlLoaderClient* loader = loaderData->m_loader;

    if (!isActive(loader))
        return;

    loader->m_resourceHandle->client()->didFinishLoading(loader->m_resourceHandle.get());
    loader->finish();
}

} // namespace android
