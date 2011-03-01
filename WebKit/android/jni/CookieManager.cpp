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
#include "WebCookieJar.h"
#include "WebCoreJni.h"
#include <JNIHelp.h>

using namespace base;
using namespace net;

namespace android {

// JNI for android.webkit.CookieManager
static const char* javaCookieManagerClass = "android/webkit/CookieManager";

static bool acceptCookie(JNIEnv*, jobject)
{
#if USE(CHROME_NETWORK_STACK)
    // This is a static method which gets the cookie policy for all WebViews. We
    // always apply the same configuration to the contexts for both regular and
    // private browsing, so expect the same result here.
    bool regularAcceptCookies = WebCookieJar::get(false)->allowCookies();
    ASSERT(regularAcceptCookies == WebCookieJar::get(true)->allowCookies());
    return regularAcceptCookies;
#else
    // The Android HTTP stack is implemented Java-side.
    ASSERT_NOT_REACHED();
    return false;
#endif
}

static jstring getCookie(JNIEnv* env, jobject, jstring url, jboolean privateBrowsing)
{
#if USE(CHROME_NETWORK_STACK)
    GURL gurl(jstringToStdString(env, url));
    CookieOptions options;
    options.set_include_httponly();
    std::string cookies = WebCookieJar::get(privateBrowsing)->cookieStore()->GetCookieMonster()->GetCookiesWithOptions(gurl, options);
    return stdStringToJstring(env, cookies);
#else
    // The Android HTTP stack is implemented Java-side.
    ASSERT_NOT_REACHED();
    return jstring();
#endif
}

static bool hasCookies(JNIEnv*, jobject, jboolean privateBrowsing)
{
#if USE(CHROME_NETWORK_STACK)
    return WebCookieJar::get(privateBrowsing)->getNumCookiesInDatabase() > 0;
#else
    // The Android HTTP stack is implemented Java-side.
    ASSERT_NOT_REACHED();
    return false;
#endif
}

static void removeAllCookie(JNIEnv*, jobject)
{
#if USE(CHROME_NETWORK_STACK)
    WebCookieJar::get(false)->cookieStore()->GetCookieMonster()->DeleteAll(true);
    // This will lazily create a new private browsing context. However, if the
    // context doesn't already exist, there's no need to create it, as cookies
    // for such contexts are cleared up when we're done with them.
    // TODO: Consider adding an optimisation to not create the context if it
    // doesn't already exist.
    WebCookieJar::get(true)->cookieStore()->GetCookieMonster()->DeleteAll(true);

    // The Java code removes cookies directly from the backing database, so we do the same,
    // but with a NULL callback so it's asynchronous.
    WebCookieJar::get(true)->cookieStore()->GetCookieMonster()->FlushStore(NULL);
#endif
}

static void removeExpiredCookie(JNIEnv*, jobject)
{
#if USE(CHROME_NETWORK_STACK)
    // This simply forces a GC. The getters delete expired cookies so won't return expired cookies anyway.
    WebCookieJar::get(false)->cookieStore()->GetCookieMonster()->GetAllCookies();
    WebCookieJar::get(true)->cookieStore()->GetCookieMonster()->GetAllCookies();
#endif
}

static void removeSessionCookies(WebCookieJar* cookieJar)
{
#if USE(CHROME_NETWORK_STACK)
  CookieMonster* cookieMonster = cookieJar->cookieStore()->GetCookieMonster();
  CookieMonster::CookieList cookies = cookieMonster->GetAllCookies();
  for (CookieMonster::CookieList::const_iterator iter = cookies.begin(); iter != cookies.end(); ++iter) {
    if (iter->IsSessionCookie())
      cookieMonster->DeleteCanonicalCookie(*iter);
  }
#endif
}

static void removeSessionCookie(JNIEnv*, jobject)
{
#if USE(CHROME_NETWORK_STACK)
  removeSessionCookies(WebCookieJar::get(false));
  removeSessionCookies(WebCookieJar::get(true));
#endif
}

static void setAcceptCookie(JNIEnv*, jobject, jboolean accept)
{
#if USE(CHROME_NETWORK_STACK)
    // This is a static method which configures the cookie policy for all
    // WebViews, so we configure the contexts for both regular and private
    // browsing.
    WebCookieJar::get(false)->setAllowCookies(accept);
    WebCookieJar::get(true)->setAllowCookies(accept);
#endif
}

static void setCookie(JNIEnv* env, jobject, jstring url, jstring value, jboolean privateBrowsing)
{
#if USE(CHROME_NETWORK_STACK)
    GURL gurl(jstringToStdString(env, url));
    std::string line(jstringToStdString(env, value));
    CookieOptions options;
    options.set_include_httponly();
    WebCookieJar::get(privateBrowsing)->cookieStore()->GetCookieMonster()->SetCookieWithOptions(gurl, line, options);
#endif
}

static void flushCookieStore(JNIEnv*, jobject)
{
#if USE(CHROME_NETWORK_STACK)
    WebCookieJar::flush();
#endif
}

static bool acceptFileSchemeCookies(JNIEnv*, jobject)
{
#if USE(CHROME_NETWORK_STACK)
    return WebCookieJar::acceptFileSchemeCookies();
#else
    // File scheme cookies are always accepted with the Android HTTP stack.
    return true;
#endif
}

static void setAcceptFileSchemeCookies(JNIEnv*, jobject, jboolean accept)
{
#if USE(CHROME_NETWORK_STACK)
    WebCookieJar::setAcceptFileSchemeCookies(accept);
#else
    // File scheme cookies are always accepted with the Android HTTP stack.
#endif
}

static JNINativeMethod gCookieManagerMethods[] = {
    { "nativeAcceptCookie", "()Z", (void*) acceptCookie },
    { "nativeGetCookie", "(Ljava/lang/String;Z)Ljava/lang/String;", (void*) getCookie },
    { "nativeHasCookies", "(Z)Z", (void*) hasCookies },
    { "nativeRemoveAllCookie", "()V", (void*) removeAllCookie },
    { "nativeRemoveExpiredCookie", "()V", (void*) removeExpiredCookie },
    { "nativeRemoveSessionCookie", "()V", (void*) removeSessionCookie },
    { "nativeSetAcceptCookie", "(Z)V", (void*) setAcceptCookie },
    { "nativeSetCookie", "(Ljava/lang/String;Ljava/lang/String;Z)V", (void*) setCookie },
    { "nativeFlushCookieStore", "()V", (void*) flushCookieStore },
    { "nativeAcceptFileSchemeCookies", "()Z", (void*) acceptFileSchemeCookies },
    { "nativeSetAcceptFileSchemeCookies", "(Z)V", (void*) setAcceptFileSchemeCookies },
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
