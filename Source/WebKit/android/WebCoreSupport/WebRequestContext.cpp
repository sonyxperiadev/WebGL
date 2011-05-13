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
#include "WebRequestContext.h"

#include "ChromiumIncludes.h"
#include "ChromiumInit.h"
#include "WebCache.h"
#include "WebCookieJar.h"

#include <wtf/text/CString.h>

static std::string acceptLanguageStdString("");
static WTF::String acceptLanguageWtfString("");
static WTF::Mutex acceptLanguageMutex;

static int numPrivateBrowsingInstances;

extern void ANPSystemInterface_CleanupIncognito();

using namespace WTF;

namespace android {

WebRequestContext::WebRequestContext(bool isPrivateBrowsing)
    : m_isPrivateBrowsing(isPrivateBrowsing)
{
    // Initialize chromium logging, needs to be done before any chromium code is called.
    initChromium();

    if (m_isPrivateBrowsing) {
        // Delete the old files if this is the first private browsing instance
        // They are probably leftovers from a power cycle
        // We do not need to clear the cache as it is in memory only for private browsing
        if (!numPrivateBrowsingInstances)
            WebCookieJar::cleanup(true);
        numPrivateBrowsingInstances++;
    }

    WebCache* cache = WebCache::get(m_isPrivateBrowsing);
    host_resolver_ = cache->hostResolver();
    http_transaction_factory_ = cache->cache();

    WebCookieJar* cookieJar = WebCookieJar::get(m_isPrivateBrowsing);
    cookie_store_ = cookieJar->cookieStore();
    cookie_policy_ = cookieJar;

    // Also hardcoded in FrameLoader.java
    accept_charset_ = "utf-8, iso-8859-1, utf-16, *;q=0.7";
}

WebRequestContext::~WebRequestContext()
{
    if (m_isPrivateBrowsing) {
        numPrivateBrowsingInstances--;

        // This is the last private browsing context, delete the cookies and cache
        if (!numPrivateBrowsingInstances) {
            WebCookieJar::cleanup(true);
            WebCache::cleanup(true);
            ANPSystemInterface_CleanupIncognito();
        }
    }
}

void WebRequestContext::setUserAgent(const String& string)
{
    MutexLocker lock(m_userAgentMutex);
    m_userAgent = string.utf8().data();
}

void WebRequestContext::setCacheMode(int mode)
{
    m_cacheMode = mode;
}

int WebRequestContext::getCacheMode()
{
    return m_cacheMode;
}

const std::string& WebRequestContext::GetUserAgent(const GURL& url) const
{
    MutexLocker lock(m_userAgentMutex);
    return m_userAgent;
}

void WebRequestContext::setAcceptLanguage(const String& string)
{
    MutexLocker lock(acceptLanguageMutex);
    acceptLanguageStdString = string.utf8().data();
    acceptLanguageWtfString = string;
}

const std::string& WebRequestContext::GetAcceptLanguage() const
{
    MutexLocker lock(acceptLanguageMutex);
    return acceptLanguageStdString;
}

const String& WebRequestContext::acceptLanguage()
{
    MutexLocker lock(acceptLanguageMutex);
    return acceptLanguageWtfString;
}

} // namespace android
