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

#include "WebUrlLoader.h"

#include "WebUrlLoaderClient.h"

namespace android {

// on main thread
WebUrlLoader::WebUrlLoader(WebCore::ResourceHandle* resourceHandle, const WebCore::ResourceRequest& resourceRequest)
{
    m_loaderClient = new WebUrlLoaderClient(resourceHandle, resourceRequest);
}

// on main thread
WebUrlLoader::~WebUrlLoader()
{
}

PassRefPtr<WebUrlLoader> WebUrlLoader::start(WebCore::ResourceHandle* resourceHandle, const WebCore::ResourceRequest& resourceRequest, bool isSync)
{
    RefPtr<WebUrlLoader> loader = WebUrlLoader::create(resourceHandle, resourceRequest);
    loader->m_loaderClient->start(isSync);

    return loader.release();
}

PassRefPtr<WebUrlLoader> WebUrlLoader::create(WebCore::ResourceHandle* resourceHandle, const WebCore::ResourceRequest& resourceRequest)
{
    return adoptRef(new WebUrlLoader(resourceHandle, resourceRequest));
}

// on main thread
void WebUrlLoader::cancel()
{
    m_loaderClient->cancel();
}

} // namespace android


namespace WebCore {
// on main thread
// static
// TODO: Implement sync requests
PassRefPtr<ResourceLoaderAndroid> ResourceLoaderAndroid::start(WebCore::ResourceHandle* resourceHandle, const WebCore::ResourceRequest& resourceRequest,
        FrameLoaderClient* /*client*/, bool /*isMainResource*/, bool isSync)
{
    return android::WebUrlLoader::start(resourceHandle, resourceRequest, isSync);
}

// static
bool ResourceLoaderAndroid::willLoadFromCache(const WebCore::KURL&, int64_t identifier)
{
    // This method is used to determine if a POST request can be repeated from
    // cache, but you cannot really know until you actually try to read from the
    // cache.  Even if we checked now, something else could come along and wipe
    // out the cache entry by the time we fetch it.
    //
    // So, we always say yes here, to prevent the FrameLoader from initiating a
    // reload.  Then in FrameLoaderClientImpl::dispatchWillSendRequest, we
    // fix-up the cache policy of the request to force a load from the cache.
    //
    return true;
}

} // namespace WebCore
