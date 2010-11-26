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
#include "WebCache.h"

#include "JNIUtility.h"
#include "WebCoreJni.h"
#include "WebRequestContext.h"
#include "WebUrlLoaderClient.h"

using namespace WTF;
using namespace net;

namespace android {

static const std::string& rootDirectory()
{
    // This method may be called on any thread, as the Java method is
    // synchronized.
    static WTF::Mutex mutex;
    MutexLocker lock(mutex);
    static std::string cacheDirectory;
    if (cacheDirectory.empty()) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        jclass bridgeClass = env->FindClass("android/webkit/JniUtil");
        jmethodID method = env->GetStaticMethodID(bridgeClass, "getCacheDirectory", "()Ljava/lang/String;");
        cacheDirectory = jstringToStdString(env, static_cast<jstring>(env->CallStaticObjectMethod(bridgeClass, method)));
        env->DeleteLocalRef(bridgeClass);
    }
    return cacheDirectory;
}

WebCache* WebCache::get(bool isPrivateBrowsing)
{
    static const char* const kDirectory = "/webviewCacheChromium";
    static const char* const kDirectoryPrivate = "/webviewCacheChromiumPrivate";

    static WebCache* regularCache = 0;
    static WebCache* privateCache = 0;

    if (isPrivateBrowsing) {
        if (!privateCache) {
            std::string storageDirectory = rootDirectory();
            storageDirectory.append(kDirectoryPrivate);
            privateCache = new WebCache(storageDirectory);
        }
        return privateCache;
    }

    if (!regularCache) {
        std::string storageDirectory = rootDirectory();
        storageDirectory.append(kDirectory);
        regularCache = new WebCache(storageDirectory);
    }
    return regularCache;
}

WebCache::WebCache(const std::string& storageDirectory)
    : m_storageDirectory(storageDirectory)
    , m_doomAllEntriesCallback(this, &WebCache::doomAllEntries)
    , m_doneCallback(this, &WebCache::onClearDone)
    , m_isClearInProgress(false)
{
    base::Thread* ioThread = WebUrlLoaderClient::ioThread();
    scoped_refptr<base::MessageLoopProxy> cacheMessageLoopProxy = ioThread->message_loop_proxy();

    static const int kMaximumCacheSizeBytes = 20 * 1024 * 1024;
    FilePath directoryPath(m_storageDirectory.c_str());
    net::HttpCache::DefaultBackend* backendFactory = new net::HttpCache::DefaultBackend(net::DISK_CACHE, directoryPath, kMaximumCacheSizeBytes, cacheMessageLoopProxy);

    m_hostResolver = net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism, 0, 0);
    m_cache = new net::HttpCache(m_hostResolver.get(),
                                 0, // dnsrr_resolver
                                 net::ProxyService::CreateDirect(),
                                 net::SSLConfigService::CreateSystemSSLConfigService(),
                                 net::HttpAuthHandlerFactory::CreateDefault(m_hostResolver.get()),
                                 0, // network_delegate
                                 0, // net_log
                                 backendFactory);
}

void WebCache::clear()
{
     base::Thread* thread = WebUrlLoaderClient::ioThread();
     if (thread)
         thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this, &WebCache::doClear));
}

void WebCache::cleanupFiles()
{
    WebRequestContext::removeFileOrDirectory(m_storageDirectory.c_str());
}

void WebCache::doClear()
{
    if (m_isClearInProgress)
        return;
    m_isClearInProgress = true;

    int code = m_cache->GetBackend(&m_cacheBackend, &m_doomAllEntriesCallback);
    // Code ERR_IO_PENDING indicates that the operation is still in progress and
    // the supplied callback will be invoked when it completes.
    if (code == ERR_IO_PENDING)
        return;
    else if (code != OK) {
        m_isClearInProgress = false;
        return;
    }
    doomAllEntries(0 /*unused*/);
}

void WebCache::doomAllEntries(int)
{
    if (!m_cacheBackend) {
        m_isClearInProgress = false;
        return;
    }

    int code = m_cacheBackend->DoomAllEntries(&m_doneCallback);
    // Code ERR_IO_PENDING indicates that the operation is still in progress and
    // the supplied callback will be invoked when it completes.
    if (code == ERR_IO_PENDING)
        return;
    else if (code != OK) {
        m_isClearInProgress = false;
        return;
    }
    onClearDone(0 /*unused*/);
}

void WebCache::onClearDone(int)
{
    m_isClearInProgress = false;
}

} // namespace android
