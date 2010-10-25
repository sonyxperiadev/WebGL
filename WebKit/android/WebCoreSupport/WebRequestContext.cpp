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
#include "JNIUtility.h"
#include "WebCoreJni.h"
#include "WebUrlLoaderClient.h"
#include "jni.h"

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wtf/text/CString.h>

namespace {
// TODO: The userAgent should not be a static, as it can be set per WebView.
// http://b/3113804
std::string userAgent("");
std::string acceptLanguage("");

Lock userAgentLock;
Lock acceptLanguageLock;

WTF::Mutex databaseDirectoryMutex;
WTF::Mutex cacheDirectoryMutex;
}

using namespace WTF;

namespace android {

static const char* const kCookiesDatabaseFilename = "/webviewCookiesChromium.db";
static const char* const kCacheDirectory = "/webviewCacheChromium";
static const char* const kCookiesDatabaseFilenamePrivate = "/webviewCookiesChromiumPrivate.db";
static const char* const kCacheDirectoryPrivate = "/webviewCacheChromiumPrivate";

static scoped_refptr<WebRequestContext> privateBrowsingContext(0);
static WTF::Mutex privateBrowsingContextMutex;

WebRequestContext::WebRequestContext()
    : m_allowCookies(true)
{
    // Also hardcoded in FrameLoader.java
    accept_charset_ = "utf-8, iso-8859-1, utf-16, *;q=0.7";
}

WebRequestContext::~WebRequestContext()
{
}

void WebRequestContext::setUserAgent(String string)
{
    // The useragent is set on the WebCore thread and read on the network
    // stack's IO thread.
    AutoLock aLock(userAgentLock);
    userAgent = string.utf8().data();
}

const std::string& WebRequestContext::GetUserAgent(const GURL& url) const
{
    // The useragent is set on the WebCore thread and read on the network
    // stack's IO thread.
    AutoLock aLock(userAgentLock);
    ASSERT(userAgent != "");
    return userAgent;
}

void WebRequestContext::setAcceptLanguage(String string)
{
    // The accept language is set on the WebCore thread and read on the network
    // stack's IO thread.
    AutoLock aLock(acceptLanguageLock);
    acceptLanguage = string.utf8().data();
}

const std::string& WebRequestContext::GetAcceptLanguage() const
{
    // The accept language is set on the WebCore thread and read on the network
    // stack's IO thread.
    AutoLock aLock(acceptLanguageLock);
    return acceptLanguage;
}

static const std::string& getDatabaseDirectory()
{
    // This method may be called on any thread, as the Java method is
    // synchronized.
    MutexLocker lock(databaseDirectoryMutex);
    static std::string databaseDirectory;
    if (databaseDirectory.empty()) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        jclass bridgeClass = env->FindClass("android/webkit/CookieSyncManager");
        jmethodID method = env->GetStaticMethodID(bridgeClass, "getDatabaseDirectory", "()Ljava/lang/String;");
        databaseDirectory = jstringToStdString(env, static_cast<jstring>(env->CallStaticObjectMethod(bridgeClass, method)));
        env->DeleteLocalRef(bridgeClass);
    }
    return databaseDirectory;
}

static const std::string& getCacheDirectory()
{
    // This method may be called on any thread, as the Java method is
    // synchronized.
    MutexLocker lock(cacheDirectoryMutex);
    static std::string cacheDirectory;
    if (cacheDirectory.empty()) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        jclass bridgeClass = env->FindClass("android/webkit/CookieSyncManager");
        jmethodID method = env->GetStaticMethodID(bridgeClass, "getCacheDirectory", "()Ljava/lang/String;");
        cacheDirectory = jstringToStdString(env, static_cast<jstring>(env->CallStaticObjectMethod(bridgeClass, method)));
        env->DeleteLocalRef(bridgeClass);
    }
    return cacheDirectory;
}

WebRequestContext* WebRequestContext::getContextForPath(const char* cookieFilename, const char* cacheFilename)
{
    std::string cookieString(getDatabaseDirectory());
    cookieString.append(cookieFilename);
    FilePath cookiePath(cookieString.c_str());
    std::string cacheString(getCacheDirectory());
    cacheString.append(cacheFilename);
    FilePath cachePath(cacheString.c_str());

    WebRequestContext* context = new WebRequestContext();
    context->host_resolver_ = net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism, 0);
    base::Thread* ioThread = WebUrlLoaderClient::ioThread();
    scoped_refptr<base::MessageLoopProxy> cacheMessageLoopProxy = ioThread->message_loop_proxy();
    // Todo: check if the context takes ownership of the cache
    net::HttpCache::DefaultBackend* defaultBackend = new net::HttpCache::DefaultBackend(net::DISK_CACHE, cachePath, 20 * 1024 * 1024, cacheMessageLoopProxy);

    context->http_transaction_factory_ = new net::HttpCache(context->host_resolver(), net::ProxyService::CreateDirect(), net::SSLConfigService::CreateSystemSSLConfigService(), net::HttpAuthHandlerFactory::CreateDefault(context->host_resolver_), 0, 0, defaultBackend);

    scoped_refptr<SQLitePersistentCookieStore> cookieDb = new SQLitePersistentCookieStore(cookiePath);

    // This is needed for the page cycler. See http://b/2944150
    net::CookieMonster::EnableFileScheme();

    context->cookie_store_ = new net::CookieMonster(cookieDb.get(), 0);
    context->cookie_policy_ = context;

    return context;
}

WebRequestContext* WebRequestContext::getRegularContext()
{
    static WTF::Mutex regularContextMutex;
    static scoped_refptr<WebRequestContext> regularContext(0);

    MutexLocker lock(regularContextMutex);
    if (!regularContext)
        regularContext = getContextForPath(kCookiesDatabaseFilename, kCacheDirectory);
    return regularContext;
}

WebRequestContext* WebRequestContext::getPrivateBrowsingContext()
{
    MutexLocker lock(privateBrowsingContextMutex);

    if (!privateBrowsingContext) {
        // TODO: Where is the right place to put the temporary db? Should it be
        // kept in memory?
        privateBrowsingContext = getContextForPath(kCookiesDatabaseFilenamePrivate, kCacheDirectoryPrivate);
    }
    return privateBrowsingContext;
}

WebRequestContext* WebRequestContext::get(bool isPrivateBrowsing)
{
    return isPrivateBrowsing ? getPrivateBrowsingContext() : getRegularContext();
}

static void removeFileOrDirectory(const char* filename)
{
    struct stat filetype;
    if (stat(filename, &filetype) != 0)
        return;
    if (S_ISDIR(filetype.st_mode)) {
        DIR* directory = opendir(filename);
        if (directory) {
            while (struct dirent* entry = readdir(directory)) {
                if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
                    continue;
                std::string entryName(filename);
                entryName.append("/");
                entryName.append(entry->d_name);
                removeFileOrDirectory(entryName.c_str());
            }
            closedir(directory);
            rmdir(filename);
        }
        return;
    }
    unlink(filename);
}

bool WebRequestContext::cleanupPrivateBrowsingFiles(const std::string& databaseDirectory, const std::string& cacheDirectory)
{
    // This is called on the UI thread.
    MutexLocker lock(privateBrowsingContextMutex);

    if (!privateBrowsingContext || privateBrowsingContext->HasOneRef()) {
        privateBrowsingContext = 0;

        std::string cookiePath(databaseDirectory);
        cookiePath.append(kCookiesDatabaseFilenamePrivate);
        removeFileOrDirectory(cookiePath.c_str());
        std::string cachePath(cacheDirectory);
        cachePath.append(kCacheDirectoryPrivate);
        removeFileOrDirectory(cachePath.c_str());
        return true;
    }
    return false;
}

int WebRequestContext::CanGetCookies(const GURL&, const GURL&, net::CompletionCallback*)
{
    MutexLocker lock(m_allowCookiesMutex);
    return m_allowCookies ? net::OK : net::ERR_ACCESS_DENIED;
}

int WebRequestContext::CanSetCookie(const GURL&, const GURL&, const std::string&, net::CompletionCallback*)
{
    MutexLocker lock(m_allowCookiesMutex);
    return m_allowCookies ? net::OK : net::ERR_ACCESS_DENIED;
}

bool WebRequestContext::allowCookies() {
    MutexLocker lock(m_allowCookiesMutex);
    return m_allowCookies;
}

void WebRequestContext::setAllowCookies(bool allow)
{
    MutexLocker lock(m_allowCookiesMutex);
    m_allowCookies = allow;
}

} // namespace android
