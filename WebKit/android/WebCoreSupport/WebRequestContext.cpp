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
#include "net/base/cookie_monster.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"

namespace {
    // TODO: Get uastring from webcore
    std::string userAgent("Mozilla/5.0 (Linux; U; Android 2.1; en-gb; Nexus One Build/ERE21) AppleWebKit/530.17 (KHTML, like Gecko) Version/4.0 Mobile Safari/530.17");
}

namespace android {

std::string* WebRequestContext::s_dataDirectory = 0;
net::HttpCache* WebRequestContext::s_cache = 0;

WebRequestContext::WebRequestContext()
{
}

WebRequestContext::~WebRequestContext()
{
}

const std::string& WebRequestContext::GetUserAgent(const GURL& url) const
{
    return userAgent;
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

// Some of the members of the RequestContext will be deleted when the URLRequest
// is deleted as they are scoped pointers. Documented in WebRequestContext.h
WebRequestContext* WebRequestContext::GetAndroidContext()
{
    WebRequestContext* androidContext = new WebRequestContext();
    androidContext->host_resolver_ = net::CreateSystemHostResolver(0);
    androidContext->cookie_store_ = new net::CookieMonster(0);

    // In memory cache
    if (!s_cache)
        s_cache = new net::HttpCache(0, androidContext->host_resolver(), 0, net::SSLConfigService::CreateSystemSSLConfigService(), 0);
    androidContext->http_transaction_factory_ = s_cache;

    return androidContext;
}

} // namespace android
