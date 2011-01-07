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

#ifndef WebCache_h
#define WebCache_h

#include "ChromiumIncludes.h"

#include <OwnPtr.h>
#include <wtf/ThreadingPrimitives.h>

namespace android {

// This class is not generally threadsafe. get() is not threadsafe - instances
// are created on the WebCore thread only.
class WebCache : public base::RefCountedThreadSafe<WebCache> {
public:
    static WebCache* get(bool isPrivateBrowsing);

    void clear();
    static void cleanup(bool isPrivateBrowsing);
    net::HostResolver* hostResolver() { return m_hostResolver.get(); }
    net::HttpCache* cache() { return m_cache.get(); }
    net::ProxyConfigServiceAndroid* proxy() { return m_proxyConfigService; }

private:
    WebCache(bool isPrivateBrowsing);

    // For clear()
    void clearImpl();
    void doomAllEntries(int);
    void onClearDone(int);

    OwnPtr<net::HostResolver> m_hostResolver;
    OwnPtr<net::HttpCache> m_cache;
    // This is owned by the ProxyService, which is owned by the HttpNetworkLayer,
    // which is owned by the HttpCache, which is owned by this class.
    net::ProxyConfigServiceAndroid* m_proxyConfigService;

    // For clear()
    net::CompletionCallbackImpl<WebCache> m_doomAllEntriesCallback;
    net::CompletionCallbackImpl<WebCache> m_onClearDoneCallback;
    disk_cache::Backend* m_cacheBackend;
    bool m_isClearInProgress;
};

} // namespace android

#endif
