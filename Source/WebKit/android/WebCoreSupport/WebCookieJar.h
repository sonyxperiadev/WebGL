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

#ifndef WebCookieJar_h
#define WebCookieJar_h

#include "ChromiumIncludes.h"

#include <wtf/ThreadingPrimitives.h>

namespace android {

// This class is threadsafe. It is used from the IO, WebCore and Chromium IO
// threads.
class WebCookieJar : public net::CookiePolicy, public base::RefCountedThreadSafe<WebCookieJar> {
public:
    static WebCookieJar* get(bool isPrivateBrowsing);
    static void cleanup(bool isPrivateBrowsing);

    // Flush all cookies to disk. Synchronous.
    static void flush();

    // CookiePolicy implementation from external/chromium
    virtual int CanGetCookies(const GURL& url, const GURL& first_party_for_cookies) const;
    virtual int CanSetCookie(const GURL& url, const GURL& first_party_for_cookies, const std::string& cookie_line) const;

    bool allowCookies();
    void setAllowCookies(bool allow);

    // Getter and setter for whether we accept cookies for file scheme URLS.
    // Defaults to false. Note that calls to the setter are ignored once the
    // first instance of this class has been created.
    static bool acceptFileSchemeCookies();
    static void setAcceptFileSchemeCookies(bool);

    // Instead of this it would probably be better to add the cookie methods
    // here so the rest of WebKit doesn't have to know about Chromium classes
    net::CookieStore* cookieStore() { return m_cookieStore.get(); }
    net::CookiePolicy* cookiePolicy() { return this; }

    // Get the number of cookies that have actually been saved to flash.
    // (This is used to implement CookieManager.hasCookies() in the Java framework.)
    int getNumCookiesInDatabase();

private:
    WebCookieJar(const std::string& databaseFilePath);

    scoped_refptr<SQLitePersistentCookieStore> m_cookieDb;
    scoped_refptr<net::CookieStore> m_cookieStore;
    bool m_allowCookies;
    mutable WTF::Mutex m_allowCookiesMutex;
};

}

#endif
