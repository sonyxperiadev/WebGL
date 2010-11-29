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
#include "WebCookieJar.h"

#include "JNIUtility.h"
#include "WebCoreJni.h"
#include "WebRequestContext.h"

namespace android {

const std::string& databaseDirectory()
{
    // This method may be called on any thread, as the Java method is
    // synchronized.
    static WTF::Mutex databaseDirectoryMutex;
    MutexLocker lock(databaseDirectoryMutex);
    static std::string databaseDirectory;
    if (databaseDirectory.empty()) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        jclass bridgeClass = env->FindClass("android/webkit/JniUtil");
        jmethodID method = env->GetStaticMethodID(bridgeClass, "getDatabaseDirectory", "()Ljava/lang/String;");
        databaseDirectory = jstringToStdString(env, static_cast<jstring>(env->CallStaticObjectMethod(bridgeClass, method)));
        env->DeleteLocalRef(bridgeClass);
    }
    return databaseDirectory;
}


WebCookieJar* WebCookieJar::get(bool isPrivateBrowsing)
{
    static const char* const kDatabaseFilename = "/webviewCookiesChromium.db";
    static const char* const kDatabaseFilenamePrivateBrowsing = "/webviewCookiesChromiumPrivate.db";

    static WebCookieJar* regularCookieManager = 0;
    static WebCookieJar* privateCookieManager = 0;

    if (isPrivateBrowsing) {
        if (!privateCookieManager) {
            std::string databaseFilePath = databaseDirectory();
            databaseFilePath.append(kDatabaseFilenamePrivateBrowsing);
            privateCookieManager = new WebCookieJar(databaseFilePath);
        }
        return privateCookieManager;
    }

    if (!regularCookieManager) {
        std::string databaseFilePath = databaseDirectory();
        databaseFilePath.append(kDatabaseFilename);
        regularCookieManager = new WebCookieJar(databaseFilePath);
    }
    return regularCookieManager;
}

WebCookieJar::WebCookieJar(const std::string& databaseFilePath)
    : m_allowCookies(true)
    , m_databaseFilePath(databaseFilePath)
{
    // This is needed for the page cycler. See http://b/2944150
    net::CookieMonster::EnableFileScheme();

    FilePath cookiePath(m_databaseFilePath.c_str());
    scoped_refptr<SQLitePersistentCookieStore> cookieDb = new SQLitePersistentCookieStore(cookiePath);
    m_cookieStore = new net::CookieMonster(cookieDb.get(), 0);
}

bool WebCookieJar::allowCookies()
{
    MutexLocker lock(m_allowCookiesMutex);
    return m_allowCookies;
}

void WebCookieJar::setAllowCookies(bool allow)
{
    MutexLocker lock(m_allowCookiesMutex);
    m_allowCookies = allow;
}

void WebCookieJar::cleanupFiles()
{
    WebRequestContext::removeFileOrDirectory(m_databaseFilePath.c_str());
}

// From CookiePolicy in chromium
int WebCookieJar::CanGetCookies(const GURL&, const GURL&, net::CompletionCallback*)
{
    MutexLocker lock(m_allowCookiesMutex);
    return m_allowCookies ? net::OK : net::ERR_ACCESS_DENIED;
}

// From CookiePolicy in chromium
int WebCookieJar::CanSetCookie(const GURL&, const GURL&, const std::string&, net::CompletionCallback*)
{
    MutexLocker lock(m_allowCookiesMutex);
    return m_allowCookies ? net::OK : net::ERR_ACCESS_DENIED;
}

}
