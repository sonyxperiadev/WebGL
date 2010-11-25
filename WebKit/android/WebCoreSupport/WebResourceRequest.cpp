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
#include "WebResourceRequest.h"

#include "ResourceRequest.h"

#include <wtf/text/CString.h>

using namespace WebCore;

namespace android {

WebResourceRequest::WebResourceRequest(const WebCore::ResourceRequest& resourceRequest)
{
    // Set the load flags based on the WebCore request.
    m_loadFlags = net::LOAD_NORMAL;
    switch (resourceRequest.cachePolicy()) {
    case ReloadIgnoringCacheData:
        m_loadFlags |= net::LOAD_VALIDATE_CACHE;
        break;
    case ReturnCacheDataElseLoad:
        m_loadFlags |= net::LOAD_PREFERRING_CACHE;
        break;
    case ReturnCacheDataDontLoad:
        m_loadFlags |= net::LOAD_ONLY_FROM_CACHE;
        break;
    case UseProtocolCachePolicy:
        break;
    }

    // TODO: We should consider setting these flags and net::LOAD_DO_NOT_SEND_AUTH_DATA
    // when FrameLoaderClient::shouldUseCredentialStorage() is false. However,
    // the required WebKit logic is not yet in place. See Chromium's
    // FrameLoaderClientImpl::shouldUseCredentialStorage().
    if (!resourceRequest.allowCookies()) {
        m_loadFlags |= net::LOAD_DO_NOT_SAVE_COOKIES;
        m_loadFlags |= net::LOAD_DO_NOT_SEND_COOKIES;
    }


    // Set the request headers
    const HTTPHeaderMap& map = resourceRequest.httpHeaderFields();
    for (HTTPHeaderMap::const_iterator it = map.begin(); it != map.end(); ++it) {
        const std::string& nameUtf8 = it->first.string().utf8().data();
        const std::string& valueUtf8 = it->second.utf8().data();

        // Skip over referrer headers found in the header map because we already
        // pulled it out as a separate parameter.  We likewise prune the UA since
        // that will be added back by the network layer.
        if (LowerCaseEqualsASCII(nameUtf8, "referer") || LowerCaseEqualsASCII(nameUtf8, "user-agent"))
            continue;

        // Skip over "Cache-Control: max-age=0" header if the corresponding
        // load flag is already specified. FrameLoader sets both the flag and
        // the extra header -- the extra header is redundant since our network
        // implementation will add the necessary headers based on load flags.
        // See http://code.google.com/p/chromium/issues/detail?id=3434.
        if ((m_loadFlags & net::LOAD_VALIDATE_CACHE) &&
            LowerCaseEqualsASCII(nameUtf8, "cache-control") && LowerCaseEqualsASCII(valueUtf8, "max-age=0"))
            continue;

        m_requestHeaders.SetHeader(nameUtf8, valueUtf8);
    }

    m_method = resourceRequest.httpMethod().utf8().data();
    m_referrer = resourceRequest.httpReferrer().utf8().data();
    m_userAgent = resourceRequest.httpUserAgent().utf8().data();

    m_url = resourceRequest.url().string().utf8().data();
}

} // namespace android
