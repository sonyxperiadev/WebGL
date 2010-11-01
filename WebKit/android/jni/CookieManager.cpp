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

#include "ChromiumIncludes.h"
#include "WebRequestContext.h"
#include "WebCoreJni.h"
#include <JNIHelp.h>

using namespace base;
using namespace net;

namespace android {

// JNI for android.webkit.CookieManager
static const char* javaCookieManagerClass = "android/webkit/CookieManager";

// Though WebRequestContext::get() is threadsafe, the context itself, in
// general, is not. The context is generally used on the HTTP stack IO
// thread, but calls to the methods of this class are made on the UI thread.
// We ensure thread safety as follows ...
// - The cookie_store() getter just returns a pointer which is only set when the
//   context is first constructed. The CookieMonster itself is threadsafe, so
//   using it from the UI thread is safe.
// - Calls to the other WebRequestContext methods used here are explicitly
//   threadsafe.

static bool useChromiumHttpStack(JNIEnv*, jobject)
{
#if USE(CHROME_NETWORK_STACK)
    return true;
#else
    return false;
#endif
}

static bool acceptCookie(JNIEnv*, jobject)
{
#if USE(CHROME_NETWORK_STACK)
    // This is a static method which gets the cookie policy for all WebViews. We
    // always apply the same configuration to the contexts for both regular and
    // private browsing, so expect the same result here.
    bool regularAcceptCookies = WebRequestContext::get(false)->allowCookies();
    ASSERT(regularAcceptCookies == WebRequestContext::get(true)->allowCookies());
    return regularAcceptCookies;
#else
    // The Android HTTP stack is implemented Java-side.
    ASSERT_NOT_REACHED();
    return false;
#endif
}

static jstring getCookie(JNIEnv* env, jobject, jstring url)
{
#if USE(CHROME_NETWORK_STACK)
    GURL gurl(jstringToStdString(env, url));
    CookieOptions options;
    options.set_include_httponly();
    std::string cookies = WebRequestContext::get(false)->cookie_store()->GetCookieMonster()->GetCookiesWithOptions(gurl, options);
    return cookies.empty() ? 0 : env->NewStringUTF(cookies.c_str());
#else
    // The Android HTTP stack is implemented Java-side.
    ASSERT_NOT_REACHED();
    return jstring();
#endif
}

static bool hasCookies(JNIEnv*, jobject)
{
#if USE(CHROME_NETWORK_STACK)
    return !WebRequestContext::get(false)->cookie_store()->GetCookieMonster()->GetAllCookies().empty();
#else
    // The Android HTTP stack is implemented Java-side.
    ASSERT_NOT_REACHED();
    return false;
#endif
}

static void removeAllCookie(JNIEnv*, jobject)
{
#if USE(CHROME_NETWORK_STACK)
    WebRequestContext::get(false)->cookie_store()->GetCookieMonster()->DeleteAllCreatedAfter(Time(), true);
    // This will lazily create a new private browsing context. However, if the
    // context doesn't already exist, there's no need to create it, as cookies
    // for such contexts are cleared up when we're done with them.
    // TODO: Consider adding an optimisation to not create the context if it
    // doesn't already exist.
    WebRequestContext::get(true)->cookie_store()->GetCookieMonster()->DeleteAllCreatedAfter(Time(), true);
#endif
}

static void removeExpiredCookie(JNIEnv*, jobject)
{
#if USE(CHROME_NETWORK_STACK)
    // This simply forces a GC. The getters delete expired cookies so won't return expired cookies anyway.
    WebRequestContext::get(false)->cookie_store()->GetCookieMonster()->GetAllCookies();
    WebRequestContext::get(true)->cookie_store()->GetCookieMonster()->GetAllCookies();
#endif
}

static void removeSessionCookies(WebRequestContext* context)
{
#if USE(CHROME_NETWORK_STACK)
  CookieMonster* cookieMonster = context->cookie_store()->GetCookieMonster();
  CookieMonster::CookieList cookies = cookieMonster->GetAllCookies();
  for (CookieMonster::CookieList::const_iterator iter = cookies.begin(); iter != cookies.end(); ++iter) {
    if (!iter->IsPersistent())
      cookieMonster->DeleteCanonicalCookie(*iter);
  }
#endif
}

static void removeSessionCookie(JNIEnv*, jobject)
{
#if USE(CHROME_NETWORK_STACK)
  removeSessionCookies(WebRequestContext::get(false));
  removeSessionCookies(WebRequestContext::get(true));
#endif
}

static void setAcceptCookie(JNIEnv*, jobject, jboolean accept)
{
#if USE(CHROME_NETWORK_STACK)
    // This is a static method which configures the cookie policy for all
    // WebViews, so we configure the contexts for both regular and private
    // browsing.
    WebRequestContext::get(false)->setAllowCookies(accept);
    WebRequestContext::get(true)->setAllowCookies(accept);
#endif
}

static void setCookie(JNIEnv* env, jobject, jstring url, jstring value)
{
#if USE(CHROME_NETWORK_STACK)
    GURL gurl(jstringToStdString(env, url));
    std::string line(jstringToStdString(env, value));
    CookieOptions options;
    options.set_include_httponly();
    WebRequestContext::get(false)->cookie_store()->GetCookieMonster()->SetCookieWithOptions(gurl, line, options);
#endif
}

static JNINativeMethod gCookieManagerMethods[] = {
    { "nativeUseChromiumHttpStack", "()Z", (void*) useChromiumHttpStack },
    { "nativeAcceptCookie", "()Z", (void*) acceptCookie },
    { "nativeGetCookie", "(Ljava/lang/String;)Ljava/lang/String;", (void*) getCookie },
    { "nativeHasCookies", "()Z", (void*) hasCookies },
    { "nativeRemoveAllCookie", "()V", (void*) removeAllCookie },
    { "nativeRemoveExpiredCookie", "()V", (void*) removeExpiredCookie },
    { "nativeRemoveSessionCookie", "()V", (void*) removeSessionCookie },
    { "nativeSetAcceptCookie", "(Z)V", (void*) setAcceptCookie },
    { "nativeSetCookie", "(Ljava/lang/String;Ljava/lang/String;)V", (void*) setCookie },
};

int registerCookieManager(JNIEnv* env)
{
#ifndef NDEBUG
    jclass cookieManager = env->FindClass(javaCookieManagerClass);
    LOG_ASSERT(cookieManager, "Unable to find class");
    env->DeleteLocalRef(cookieManager);
#endif
    return jniRegisterNativeMethods(env, javaCookieManagerClass, gCookieManagerMethods, NELEM(gCookieManagerMethods));
}

} // namespace android
