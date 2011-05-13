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
#include "WebUrlLoaderClient.h"

#include <cutils/log.h>
#include <dirent.h>

#undef ASSERT
#define ASSERT(assertion, ...) do \
    if (!(assertion)) { \
        android_printLog(ANDROID_LOG_ERROR, __FILE__, __VA_ARGS__); \
    } \
while (0)

namespace android {

static WTF::Mutex instanceMutex;
static bool isFirstInstanceCreated = false;
static bool fileSchemeCookiesEnabled = false;

static const std::string& databaseDirectory()
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

static std::string databaseDirectory(bool isPrivateBrowsing)
{
    static const char* const kDatabaseFilename = "/webviewCookiesChromium.db";
    static const char* const kDatabaseFilenamePrivateBrowsing = "/webviewCookiesChromiumPrivate.db";

    std::string databaseFilePath = databaseDirectory();
    databaseFilePath.append(isPrivateBrowsing ? kDatabaseFilenamePrivateBrowsing : kDatabaseFilename);
    return databaseFilePath;
}

scoped_refptr<WebCookieJar>* instance(bool isPrivateBrowsing)
{
    static scoped_refptr<WebCookieJar> regularInstance;
    static scoped_refptr<WebCookieJar> privateInstance;
    return isPrivateBrowsing ? &privateInstance : &regularInstance;
}

WebCookieJar* WebCookieJar::get(bool isPrivateBrowsing)
{
    MutexLocker lock(instanceMutex);
    if (!isFirstInstanceCreated && fileSchemeCookiesEnabled)
        net::CookieMonster::EnableFileScheme();
    isFirstInstanceCreated = true;
    scoped_refptr<WebCookieJar>* instancePtr = instance(isPrivateBrowsing);
    if (!instancePtr->get())
        *instancePtr = new WebCookieJar(databaseDirectory(isPrivateBrowsing));
    return instancePtr->get();
}

void WebCookieJar::cleanup(bool isPrivateBrowsing)
{
    // This is called on the UI thread.
    MutexLocker lock(instanceMutex);
    scoped_refptr<WebCookieJar>* instancePtr = instance(isPrivateBrowsing);
    *instancePtr = 0;
    removeFileOrDirectory(databaseDirectory(isPrivateBrowsing).c_str());
}

WebCookieJar::WebCookieJar(const std::string& databaseFilePath)
    : m_allowCookies(true)
{
    // Setup the permissions for the file
    const char* cDatabasePath = databaseFilePath.c_str();
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    if (access(cDatabasePath, F_OK) == 0)
        chmod(cDatabasePath, mode);
    else {
        int fd = open(cDatabasePath, O_CREAT, mode);
        if (fd >= 0)
            close(fd);
    }

    FilePath cookiePath(databaseFilePath.c_str());
    m_cookieDb = new SQLitePersistentCookieStore(cookiePath);
    m_cookieStore = new net::CookieMonster(m_cookieDb.get(), 0);
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

int WebCookieJar::getNumCookiesInDatabase()
{
    if (!m_cookieStore)
        return 0;
    return m_cookieStore->GetCookieMonster()->GetAllCookies().size();
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

class FlushSemaphore : public base::RefCounted<FlushSemaphore>
{
public:
    FlushSemaphore()
        : m_condition(&m_lock)
        , m_count(0)
    {}

    void SendFlushRequest(net::CookieMonster* monster) {
        // FlushStore() needs to run on a Chrome thread (because it will need
        // to post the callback, and it may want to do so on its own thread.)
        // We use the IO thread for this purpose.
        //
        // TODO(husky): Our threads are hidden away in various files. Clean this
        // up and consider integrating with Chrome's browser_thread.h. Might be
        // a better idea to use the DB thread here rather than the IO thread.

        base::Thread* ioThread = WebUrlLoaderClient::ioThread();
        if (ioThread) {
            Task* callback = NewRunnableMethod(this, &FlushSemaphore::Callback);
            ioThread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
                monster, &net::CookieMonster::FlushStore, callback));
        } else {
            Callback();
        }
    }

    // Block until the given number of callbacks has been made.
    void Wait(int numCallbacks) {
        AutoLock al(m_lock);
        int lastCount = m_count;
        while (m_count < numCallbacks) {
            // TODO(husky): Maybe use TimedWait() here? But it's not obvious what
            // to do if the flush fails. Might be okay just to let the OS kill us.
            m_condition.Wait();
            ASSERT(lastCount != m_count, "Wait finished without incrementing m_count %d %d", m_count, lastCount);
            lastCount = m_count;
        }
        m_count -= numCallbacks;
    }

private:
    friend class base::RefCounted<FlushSemaphore>;

    void Callback() {
        AutoLock al(m_lock);
        m_count++;
        m_condition.Broadcast();
    }

    Lock m_lock;
    ConditionVariable m_condition;
    volatile int m_count;
};

void WebCookieJar::flush()
{
    // Flush both cookie stores (private and non-private), wait for 2 callbacks.
    static scoped_refptr<FlushSemaphore> semaphore(new FlushSemaphore());
    semaphore->SendFlushRequest(get(false)->cookieStore()->GetCookieMonster());
    semaphore->SendFlushRequest(get(true)->cookieStore()->GetCookieMonster());
    semaphore->Wait(2);
}

bool WebCookieJar::acceptFileSchemeCookies()
{
    MutexLocker lock(instanceMutex);
    return fileSchemeCookiesEnabled;
}

void WebCookieJar::setAcceptFileSchemeCookies(bool accept)
{
    // The Chromium HTTP stack only reflects changes to this flag when creating
    // a new CookieMonster instance. While we could track whether any
    // CookieMonster instances currently exist, this would be complicated and is
    // not required, so we only allow this flag to be changed before the first
    // instance is created.
    MutexLocker lock(instanceMutex);
    if (!isFirstInstanceCreated)
        fileSchemeCookiesEnabled = accept;
}

}
