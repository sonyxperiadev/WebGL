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

#ifndef WebRequestContext_h
#define WebRequestContext_h

#include "ChromiumIncludes.h"
#include "PlatformString.h"

#include <wtf/ThreadingPrimitives.h>

namespace android {

class WebRequestContext : public URLRequestContext, net::CookiePolicy {
public:
    // URLRequestContext overrides.
    virtual const std::string& GetUserAgent(const GURL&) const;
    virtual const std::string& GetAcceptLanguage() const;

    // CookiePolicy implementation.
    virtual int CanGetCookies(const GURL& url, const GURL& first_party_for_cookies, net::CompletionCallback*);
    virtual int CanSetCookie(const GURL& url, const GURL& first_party_for_cookies, const std::string& cookie_line, net::CompletionCallback*);

    // Lazily create the relevant context. This class holds a reference.
    // This may be called on any thread. The context returned, however, is not
    // threadsafe, and should only be used on a single thread (the network stack
    // IO thread), with the exception of the methods below.
    static WebRequestContext* get(bool isPrivateBrowsing);

    // These methods are threadsafe.
    static bool cleanupPrivateBrowsingFiles();
    static void setUserAgent(WTF::String);
    static void setAcceptLanguage(WTF::String);
    bool allowCookies();
    void setAllowCookies(bool allow);

private:
    WebRequestContext();
    ~WebRequestContext();

    static WebRequestContext* getContextForPath(const char* cookieFilename, const char* cacheFilename);
    static WebRequestContext* getRegularContext();
    static WebRequestContext* getPrivateBrowsingContext();

    bool m_allowCookies;
    WTF::Mutex m_allowCookiesMutex;
};

} // namespace android

#endif
