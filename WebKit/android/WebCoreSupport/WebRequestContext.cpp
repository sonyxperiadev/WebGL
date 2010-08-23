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

#include "JNIUtility.h"
#include "jni.h"
#include <base/message_loop_proxy.h>
#include <chrome/browser/net/sqlite_persistent_cookie_store.h>
#include <net/base/cookie_monster.h>
#include <net/base/ssl_config_service.h>
#include <net/http/http_cache.h>
#include <net/http/http_network_layer.h>
#include <net/proxy/proxy_service.h>
#include <wtf/text/CString.h>

namespace {
std::string userAgent("Mozilla/5.0 (Linux; U; Android 2.1; en-gb; Nexus One Build/ERE21) AppleWebKit/530.17 (KHTML, like Gecko) Version/4.0 Mobile Safari/530.17");
std::string acceptLanguage("");

Lock userAgentLock;
Lock acceptLanguageLock;
}

namespace android {

std::string* WebRequestContext::s_dataDirectory(0);

WebRequestContext::WebRequestContext()
{
    // Also hardcoded in FrameLoader.java
    accept_charset_ = "utf-8, iso-8859-1, utf-16, *;q=0.7";
}

WebRequestContext::~WebRequestContext()
{
}

void WebRequestContext::SetUserAgent(WTF::String string)
{
    AutoLock aLock(userAgentLock);
    userAgent = string.utf8().data();
    userAgent.append(" alternate http");
}

const std::string& WebRequestContext::GetUserAgent(const GURL& url) const
{
    AutoLock aLock(userAgentLock);
    return userAgent;
}

void WebRequestContext::SetAcceptLanguage(WTF::String string)
{
    AutoLock aLock(acceptLanguageLock);
    acceptLanguage = string.utf8().data();
}

const std::string& WebRequestContext::GetAcceptLanguage() const
{
    AutoLock aLock(acceptLanguageLock);
    return acceptLanguage;
}

const std::string* WebRequestContext::GetDataDirectory()
{
    if (!s_dataDirectory) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        jclass bridgeClass = env->FindClass("android/webkit/BrowserFrame");
        jmethodID method = env->GetStaticMethodID(bridgeClass, "getDataDirectory", "()Ljava/lang/String;");

        jstring str = (jstring)env->CallStaticObjectMethod(bridgeClass, method);
        jboolean isCopy;
        const char* nativeString = env->GetStringUTFChars(str, &isCopy);
        s_dataDirectory = new std::string(nativeString);
        if (isCopy == JNI_TRUE)
            env->ReleaseStringUTFChars(str, nativeString);
    }

    return s_dataDirectory;
}

WebRequestContext* WebRequestContext::GetAndroidContextForPath(const char* cookieFilename, const char* cacheFilename)
{
    std::string cookieString(*GetDataDirectory());
    cookieString.append(cookieFilename);
    FilePath cookiePath(cookieString.c_str());
    std::string cacheString(*GetDataDirectory());
    cacheString.append(cacheFilename);
    FilePath cachePath(cacheString.c_str());

    scoped_refptr<WebRequestContext> androidContext = new WebRequestContext();
    androidContext->host_resolver_ = net::CreateSystemHostResolver(0);
    scoped_refptr<base::MessageLoopProxy> cacheMessageLoopProxy = base::MessageLoopProxy::CreateForCurrentThread();
    // Todo: check if the context takes ownership of the cache
    net::HttpCache::DefaultBackend* defaultBackend = new net::HttpCache::DefaultBackend(net::DISK_CACHE, cachePath, 20 * 1024 * 1024, cacheMessageLoopProxy);
    androidContext->http_transaction_factory_ = new net::HttpCache(androidContext->host_resolver(), net::ProxyService::CreateNull(), net::SSLConfigService::CreateSystemSSLConfigService(), 0, 0, 0, defaultBackend);

    scoped_refptr<SQLitePersistentCookieStore> cookieDb = new SQLitePersistentCookieStore(cookiePath);

    // This is needed for the page cycler
    net::CookieMonster::EnableFileScheme();

    androidContext->cookie_store_ = new net::CookieMonster(cookieDb.get(), 0);

    return androidContext.release();
}

WebRequestContext* WebRequestContext::GetAndroidContext()
{
    static scoped_refptr<WebRequestContext> androidContext(0);
    if (!androidContext)
        androidContext = GetAndroidContextForPath("/chromecookies.db", "/chromecache");
    return androidContext;
}

WebRequestContext* WebRequestContext::GetAndroidPrivateBrowsingContext()
{
    static scoped_refptr<WebRequestContext> androidContext(0);
    if (!androidContext) {
        // TODO: Where is the right place to put the temporary db? Should it be
        // kept in memory?
        androidContext = GetAndroidContextForPath("/chromecookies_private.db", "/chromecache_private");
    }
    return androidContext;
}

} // namespace android
