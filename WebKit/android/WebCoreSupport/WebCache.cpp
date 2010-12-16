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

static WTF::Mutex instanceMutex;

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

static std::string storageDirectory()
{
    // Private cache is currently in memory only
    static const char* const kDirectory = "/webviewCacheChromium";
    std::string storageDirectory = rootDirectory();
    storageDirectory.append(kDirectory);
    return storageDirectory;
}

static scoped_refptr<WebCache>* instance(bool isPrivateBrowsing)
{
    static scoped_refptr<WebCache> regularInstance;
    static scoped_refptr<WebCache> privateInstance;
    return isPrivateBrowsing ? &privateInstance : &regularInstance;
}

WebCache* WebCache::get(bool isPrivateBrowsing)
{
    MutexLocker lock(instanceMutex);
    scoped_refptr<WebCache>* instancePtr = instance(isPrivateBrowsing);
    if (!instancePtr->get())
        *instancePtr = new WebCache(isPrivateBrowsing);
    return instancePtr->get();
}

WebCache::~WebCache()
{
    // We currently leak the HostResolver object to avoid a crash.
    // TODO: Fix this. See b/3243797
    m_hostResolver.leakPtr();
}

WebCache::WebCache(bool isPrivateBrowsing)
    : m_doomAllEntriesCallback(this, &WebCache::doomAllEntries)
    , m_doneCallback(this, &WebCache::onClearDone)
    , m_isClearInProgress(false)
{
    base::Thread* ioThread = WebUrlLoaderClient::ioThread();
    scoped_refptr<base::MessageLoopProxy> cacheMessageLoopProxy = ioThread->message_loop_proxy();

    static const int kMaximumCacheSizeBytes = 20 * 1024 * 1024;
    m_hostResolver = net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism, 0, 0);

    m_proxyConfigService = new ProxyConfigServiceAndroid();
    net::HttpCache::BackendFactory* backendFactory;
    if (isPrivateBrowsing)
        backendFactory = net::HttpCache::DefaultBackend::InMemory(kMaximumCacheSizeBytes / 2);
    else {
        FilePath directoryPath(storageDirectory().c_str());
        backendFactory = new net::HttpCache::DefaultBackend(net::DISK_CACHE, directoryPath, kMaximumCacheSizeBytes, cacheMessageLoopProxy);
    }

    m_cache = new net::HttpCache(m_hostResolver.get(),
                                 0, // dnsrr_resolver
                                 net::ProxyService::CreateWithoutProxyResolver(m_proxyConfigService, 0 /* net_log */),
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
