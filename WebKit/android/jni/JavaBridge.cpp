/*
 * Copyright 2006, The Android Open Source Project
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

#define LOG_TAG "webcoreglue"

#include "config.h"

#include "MemoryCache.h"
#include "Connection.h"
#include "CookieClient.h"
#include "FileSystemClient.h"
#include "JavaSharedClient.h"
#include "KeyGeneratorClient.h"
#include "KURL.h"
#include "NetworkStateNotifier.h"
#include "PackageNotifier.h"
#include "Page.h"
#include "PluginClient.h"
#include "PluginDatabase.h"
#include "Timer.h"
#include "TimerClient.h"
#ifdef ANDROID_INSTRUMENT
#include "TimeCounter.h"
#endif
#include "WebCache.h"
#include "WebCoreJni.h"

#include <JNIHelp.h>
#include <JNIUtility.h>
#include <SkUtils.h>
#include <jni.h>
#include <utils/misc.h>
#include <wtf/Platform.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/AtomicString.h>

namespace android {

// ----------------------------------------------------------------------------

static jfieldID gJavaBridge_ObjectID;

// ----------------------------------------------------------------------------
   
class JavaBridge : public TimerClient, public CookieClient, public PluginClient, public KeyGeneratorClient, public FileSystemClient
{
public:
    JavaBridge(JNIEnv* env, jobject obj);
    virtual ~JavaBridge();

    /*
     * WebCore -> Java API
     */
    virtual void setSharedTimer(long long timemillis);
    virtual void stopSharedTimer();

    virtual void setCookies(WebCore::KURL const& url, WTF::String const& value);
    virtual WTF::String cookies(WebCore::KURL const& url);
    virtual bool cookiesEnabled();

    virtual WTF::Vector<WTF::String> getPluginDirectories();
    virtual WTF::String getPluginSharedDataDirectory();

    virtual WTF::Vector<String> getSupportedKeyStrengthList();
    virtual WTF::String getSignedPublicKeyAndChallengeString(unsigned index,
            const WTF::String& challenge, const WebCore::KURL& url);
    virtual WTF::String resolveFilePathForContentUri(const WTF::String& uri);

    ////////////////////////////////////////////

    virtual void setSharedTimerCallback(void (*f)());

    ////////////////////////////////////////////

    virtual void signalServiceFuncPtrQueue();

    // jni functions
    static void Constructor(JNIEnv* env, jobject obj);
    static void Finalize(JNIEnv* env, jobject obj);
    static void SharedTimerFired(JNIEnv* env, jobject);
    static void SetCacheSize(JNIEnv* env, jobject obj, jint bytes);
    static void SetNetworkOnLine(JNIEnv* env, jobject obj, jboolean online);
    static void SetNetworkType(JNIEnv* env, jobject obj, jstring type, jstring subtype);
    static void SetDeferringTimers(JNIEnv* env, jobject obj, jboolean defer);
    static void ServiceFuncPtrQueue(JNIEnv*);
    static void UpdatePluginDirectories(JNIEnv* env, jobject obj, jobjectArray array, jboolean reload);
    static void AddPackageNames(JNIEnv* env, jobject obj, jobject packageNames);
    static void AddPackageName(JNIEnv* env, jobject obj, jstring packageName);
    static void RemovePackageName(JNIEnv* env, jobject obj, jstring packageName);
    static void UpdateProxy(JNIEnv* env, jobject obj, jstring newProxy, jstring newExList);


private:
    jweak       mJavaObject;
    jmethodID   mSetSharedTimer;
    jmethodID   mStopSharedTimer;
    jmethodID   mSetCookies;
    jmethodID   mCookies;
    jmethodID   mCookiesEnabled;
    jmethodID   mGetPluginDirectories;
    jmethodID   mGetPluginSharedDataDirectory;
    jmethodID   mSignalFuncPtrQueue;
    jmethodID   mGetKeyStrengthList;
    jmethodID   mGetSignedPublicKey;
    jmethodID   mResolveFilePathForContentUri;
};

static void (*sSharedTimerFiredCallback)();

JavaBridge::JavaBridge(JNIEnv* env, jobject obj)
{
    mJavaObject = env->NewWeakGlobalRef(obj);
    jclass clazz = env->GetObjectClass(obj);

    mSetSharedTimer = env->GetMethodID(clazz, "setSharedTimer", "(J)V");
    mStopSharedTimer = env->GetMethodID(clazz, "stopSharedTimer", "()V");
    mSetCookies = env->GetMethodID(clazz, "setCookies", "(Ljava/lang/String;Ljava/lang/String;)V");
    mCookies = env->GetMethodID(clazz, "cookies", "(Ljava/lang/String;)Ljava/lang/String;");
    mCookiesEnabled = env->GetMethodID(clazz, "cookiesEnabled", "()Z");
    mGetPluginDirectories = env->GetMethodID(clazz, "getPluginDirectories", "()[Ljava/lang/String;");
    mGetPluginSharedDataDirectory = env->GetMethodID(clazz, "getPluginSharedDataDirectory", "()Ljava/lang/String;");
    mSignalFuncPtrQueue = env->GetMethodID(clazz, "signalServiceFuncPtrQueue", "()V");
    mGetKeyStrengthList = env->GetMethodID(clazz, "getKeyStrengthList", "()[Ljava/lang/String;");
    mGetSignedPublicKey = env->GetMethodID(clazz, "getSignedPublicKey", "(ILjava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    mResolveFilePathForContentUri = env->GetMethodID(clazz, "resolveFilePathForContentUri", "(Ljava/lang/String;)Ljava/lang/String;");
    env->DeleteLocalRef(clazz);

    LOG_ASSERT(mSetSharedTimer, "Could not find method setSharedTimer");
    LOG_ASSERT(mStopSharedTimer, "Could not find method stopSharedTimer");
    LOG_ASSERT(mSetCookies, "Could not find method setCookies");
    LOG_ASSERT(mCookies, "Could not find method cookies");
    LOG_ASSERT(mCookiesEnabled, "Could not find method cookiesEnabled");
    LOG_ASSERT(mGetPluginDirectories, "Could not find method getPluginDirectories");
    LOG_ASSERT(mGetPluginSharedDataDirectory, "Could not find method getPluginSharedDataDirectory");
    LOG_ASSERT(mGetKeyStrengthList, "Could not find method getKeyStrengthList");
    LOG_ASSERT(mGetSignedPublicKey, "Could not find method getSignedPublicKey");

    JavaSharedClient::SetTimerClient(this);
    JavaSharedClient::SetCookieClient(this);
    JavaSharedClient::SetPluginClient(this);
    JavaSharedClient::SetKeyGeneratorClient(this);
    JavaSharedClient::SetFileSystemClient(this);
}

JavaBridge::~JavaBridge()
{
    if (mJavaObject) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        env->DeleteWeakGlobalRef(mJavaObject);
        mJavaObject = 0;
    }
    
    JavaSharedClient::SetTimerClient(NULL);
    JavaSharedClient::SetCookieClient(NULL);
    JavaSharedClient::SetPluginClient(NULL);
    JavaSharedClient::SetKeyGeneratorClient(NULL);
    JavaSharedClient::SetFileSystemClient(NULL);
}

void
JavaBridge::setSharedTimer(long long timemillis)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = getRealObject(env, mJavaObject);
    env->CallVoidMethod(obj.get(), mSetSharedTimer, timemillis);
}

void
JavaBridge::stopSharedTimer()
{    
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = getRealObject(env, mJavaObject);
    env->CallVoidMethod(obj.get(), mStopSharedTimer);
}

void
JavaBridge::setCookies(WebCore::KURL const& url, WTF::String const& value)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    const WTF::String& urlStr = url.string();
    jstring jUrlStr = wtfStringToJstring(env, urlStr);
    jstring jValueStr = wtfStringToJstring(env, value);

    AutoJObject obj = getRealObject(env, mJavaObject);
    env->CallVoidMethod(obj.get(), mSetCookies, jUrlStr, jValueStr);
    env->DeleteLocalRef(jUrlStr);
    env->DeleteLocalRef(jValueStr);
}

WTF::String
JavaBridge::cookies(WebCore::KURL const& url)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    const WTF::String& urlStr = url.string();
    jstring jUrlStr = wtfStringToJstring(env, urlStr);

    AutoJObject obj = getRealObject(env, mJavaObject);
    jstring string = (jstring)(env->CallObjectMethod(obj.get(), mCookies, jUrlStr));
    
    WTF::String ret = jstringToWtfString(env, string);
    env->DeleteLocalRef(jUrlStr);
    env->DeleteLocalRef(string);
    return ret;
}

bool
JavaBridge::cookiesEnabled()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = getRealObject(env, mJavaObject);
    jboolean ret = env->CallBooleanMethod(obj.get(), mCookiesEnabled);
    return (ret != 0);
}

WTF::Vector<WTF::String>
JavaBridge::getPluginDirectories()
{
    WTF::Vector<WTF::String> directories;
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = getRealObject(env, mJavaObject);
    jobjectArray array = (jobjectArray)
            env->CallObjectMethod(obj.get(), mGetPluginDirectories);
    int count = env->GetArrayLength(array);
    for (int i = 0; i < count; i++) {
        jstring dir = (jstring) env->GetObjectArrayElement(array, i);
        directories.append(jstringToWtfString(env, dir));
        env->DeleteLocalRef(dir);
    }
    env->DeleteLocalRef(array);
    checkException(env);
    return directories;
}

WTF::String
JavaBridge::getPluginSharedDataDirectory()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = getRealObject(env, mJavaObject);
    jstring ret = (jstring)env->CallObjectMethod(obj.get(), mGetPluginSharedDataDirectory);
    WTF::String path = jstringToWtfString(env, ret);
    checkException(env);
    return path;
}

void
JavaBridge::setSharedTimerCallback(void (*f)())
{
    LOG_ASSERT(!sSharedTimerFiredCallback || sSharedTimerFiredCallback==f,
               "Shared timer callback may already be set or null!");

    sSharedTimerFiredCallback = f;
}
    
void JavaBridge::signalServiceFuncPtrQueue()
{
    // In order to signal the main thread we must go through JNI. This
    // is the only usage on most threads, so we need to ensure a JNI
    // environment is setup.
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = getRealObject(env, mJavaObject);
    env->CallVoidMethod(obj.get(), mSignalFuncPtrQueue);
}

WTF::Vector<WTF::String>JavaBridge::getSupportedKeyStrengthList() {
    WTF::Vector<WTF::String> list;
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = getRealObject(env, mJavaObject);
    jobjectArray array = (jobjectArray) env->CallObjectMethod(obj.get(),
            mGetKeyStrengthList);
    int count = env->GetArrayLength(array);
    for (int i = 0; i < count; ++i) {
        jstring keyStrength = (jstring) env->GetObjectArrayElement(array, i);
        list.append(jstringToWtfString(env, keyStrength));
        env->DeleteLocalRef(keyStrength);
    }
    env->DeleteLocalRef(array);
    checkException(env);
    return list;
}

WTF::String JavaBridge::getSignedPublicKeyAndChallengeString(unsigned index,
        const WTF::String& challenge, const WebCore::KURL& url) {
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jstring jChallenge = wtfStringToJstring(env, challenge);
    const WTF::String& urlStr = url.string();
    jstring jUrl = wtfStringToJstring(env, urlStr);
    AutoJObject obj = getRealObject(env, mJavaObject);
    jstring key = (jstring) env->CallObjectMethod(obj.get(),
            mGetSignedPublicKey, index, jChallenge, jUrl);
    WTF::String ret = jstringToWtfString(env, key);
    env->DeleteLocalRef(jChallenge);
    env->DeleteLocalRef(jUrl);
    env->DeleteLocalRef(key);
    return ret;
}

WTF::String JavaBridge::resolveFilePathForContentUri(const WTF::String& uri) {
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jstring jUri = wtfStringToJstring(env, uri);
    AutoJObject obj = getRealObject(env, mJavaObject);
    jstring path = static_cast<jstring>(env->CallObjectMethod(obj.get(), mResolveFilePathForContentUri, jUri));
    WTF::String ret = jstringToWtfString(env, path);
    env->DeleteLocalRef(jUri);
    env->DeleteLocalRef(path);
    return ret;
}

// ----------------------------------------------------------------------------

void JavaBridge::Constructor(JNIEnv* env, jobject obj)
{
    JavaBridge* javaBridge = new JavaBridge(env, obj);
    env->SetIntField(obj, gJavaBridge_ObjectID, (jint)javaBridge);
}

void JavaBridge::Finalize(JNIEnv* env, jobject obj)
{
    JavaBridge* javaBridge = (JavaBridge*)
        (env->GetIntField(obj, gJavaBridge_ObjectID));    
    LOG_ASSERT(javaBridge, "Finalize should not be called twice for the same java bridge!");
    LOGV("webcore_javabridge::nativeFinalize(%p)\n", javaBridge);
    delete javaBridge;
    env->SetIntField(obj, gJavaBridge_ObjectID, 0);
}

// we don't use the java bridge object, as we're just looking at a global
void JavaBridge::SharedTimerFired(JNIEnv* env, jobject)
{
    if (sSharedTimerFiredCallback)
    {
#ifdef ANDROID_INSTRUMENT
        TimeCounter::start(TimeCounter::SharedTimerTimeCounter);
#endif
        SkAutoMemoryUsageProbe  mup("JavaBridge::sharedTimerFired");
        sSharedTimerFiredCallback();
#ifdef ANDROID_INSTRUMENT
        TimeCounter::record(TimeCounter::SharedTimerTimeCounter, __FUNCTION__);
#endif
    }
}

void JavaBridge::SetCacheSize(JNIEnv* env, jobject obj, jint bytes)
{
    WebCore::cache()->setCapacities(0, bytes/2, bytes);
}

void JavaBridge::SetNetworkOnLine(JNIEnv* env, jobject obj, jboolean online)
{
	WebCore::networkStateNotifier().networkStateChange(online);
}

void JavaBridge::SetNetworkType(JNIEnv* env, jobject obj, jstring javatype, jstring javasubtype)
{
    DEFINE_STATIC_LOCAL(AtomicString, wifi, ("wifi"));
    DEFINE_STATIC_LOCAL(AtomicString, mobile, ("mobile"));
    DEFINE_STATIC_LOCAL(AtomicString, mobileSupl, ("mobile_supl"));
    DEFINE_STATIC_LOCAL(AtomicString, gprs, ("gprs"));
    DEFINE_STATIC_LOCAL(AtomicString, edge, ("edge"));
    DEFINE_STATIC_LOCAL(AtomicString, umts, ("umts"));

    String type = jstringToWtfString(env, javatype);
    String subtype = jstringToWtfString(env, javasubtype);
    Connection::ConnectionType connectionType = Connection::UNKNOWN;
    if (type == wifi)
        connectionType = Connection::WIFI;
    else if (type == mobile || type == mobileSupl) {
        if (subtype == edge || subtype == gprs)
            connectionType = Connection::CELL_2G;
        else if (subtype == umts)
            connectionType = Connection::CELL_3G;
    }
    WebCore::networkStateNotifier().networkTypeChange(connectionType);
}

void JavaBridge::ServiceFuncPtrQueue(JNIEnv*)
{
    JavaSharedClient::ServiceFunctionPtrQueue();
}

void JavaBridge::UpdatePluginDirectories(JNIEnv* env, jobject obj,
        jobjectArray array, jboolean reload) {
    WTF::Vector<WTF::String> directories;
    int count = env->GetArrayLength(array);
    for (int i = 0; i < count; i++) {
        jstring dir = (jstring) env->GetObjectArrayElement(array, i);
        directories.append(jstringToWtfString(env, dir));
        env->DeleteLocalRef(dir);
    }
    checkException(env);
    WebCore::PluginDatabase *pluginDatabase =
            WebCore::PluginDatabase::installedPlugins();
    pluginDatabase->setPluginDirectories(directories);
    // refreshPlugins() should refresh both PluginDatabase and Page's PluginData
    WebCore::Page::refreshPlugins(reload);
}

void JavaBridge::AddPackageNames(JNIEnv* env, jobject obj, jobject packageNames)
{
    if (!packageNames)
        return;

    // dalvikvm will raise exception if any of these fail
    jclass setClass = env->FindClass("java/util/Set");
    jmethodID iterator = env->GetMethodID(setClass, "iterator",
            "()Ljava/util/Iterator;");
    jobject iter = env->CallObjectMethod(packageNames, iterator);

    jclass iteratorClass = env->FindClass("java/util/Iterator");
    jmethodID hasNext = env->GetMethodID(iteratorClass, "hasNext", "()Z");
    jmethodID next = env->GetMethodID(iteratorClass, "next", "()Ljava/lang/Object;");

    HashSet<WTF::String> namesSet;
    while (env->CallBooleanMethod(iter, hasNext)) {
        jstring name = static_cast<jstring>(env->CallObjectMethod(iter, next));
        namesSet.add(jstringToWtfString(env, name));
        env->DeleteLocalRef(name);
    }

    packageNotifier().addPackageNames(namesSet);

    env->DeleteLocalRef(iteratorClass);
    env->DeleteLocalRef(iter);
    env->DeleteLocalRef(setClass);
}

void JavaBridge::AddPackageName(JNIEnv* env, jobject obj, jstring packageName)
{
    packageNotifier().addPackageName(jstringToWtfString(env, packageName));
}

void JavaBridge::RemovePackageName(JNIEnv* env, jobject obj, jstring packageName)
{
    packageNotifier().removePackageName(jstringToWtfString(env, packageName));
}

void JavaBridge::UpdateProxy(JNIEnv* env, jobject obj, jstring newProxy, jstring newExList)
{
#if USE(CHROME_NETWORK_STACK)
    std::string proxy = jstringToStdString(env, newProxy);
    std::string exList = jstringToStdString(env, newExList);
    WebCache::get(false)->proxy()->UpdateProxySettings(proxy, exList);
    WebCache::get(true)->proxy()->UpdateProxySettings(proxy, exList);
#endif
}


// ----------------------------------------------------------------------------

/*
 * JNI registration.
 */
static JNINativeMethod gWebCoreJavaBridgeMethods[] = {
    /* name, signature, funcPtr */
    { "nativeConstructor", "()V",
        (void*) JavaBridge::Constructor },
    { "nativeFinalize", "()V",
        (void*) JavaBridge::Finalize },
    { "sharedTimerFired", "()V",
        (void*) JavaBridge::SharedTimerFired },
    { "setCacheSize", "(I)V",
        (void*) JavaBridge::SetCacheSize },
    { "setNetworkOnLine", "(Z)V",
        (void*) JavaBridge::SetNetworkOnLine },
    { "setNetworkType", "(Ljava/lang/String;Ljava/lang/String;)V",
        (void*) JavaBridge::SetNetworkType },
    { "nativeServiceFuncPtrQueue", "()V",
        (void*) JavaBridge::ServiceFuncPtrQueue },
    { "nativeUpdatePluginDirectories", "([Ljava/lang/String;Z)V",
        (void*) JavaBridge::UpdatePluginDirectories },
    { "addPackageNames", "(Ljava/util/Set;)V",
        (void*) JavaBridge::AddPackageNames },
    { "addPackageName", "(Ljava/lang/String;)V",
        (void*) JavaBridge::AddPackageName },
    { "removePackageName", "(Ljava/lang/String;)V",
        (void*) JavaBridge::RemovePackageName },
    { "nativeUpdateProxy", "(Ljava/lang/String;Ljava/lang/String;)V",
        (void*) JavaBridge::UpdateProxy }
};

int registerJavaBridge(JNIEnv* env)
{
    jclass javaBridge = env->FindClass("android/webkit/JWebCoreJavaBridge");
    LOG_FATAL_IF(javaBridge == NULL, "Unable to find class android/webkit/JWebCoreJavaBridge");
    gJavaBridge_ObjectID = env->GetFieldID(javaBridge, "mNativeBridge", "I");
    LOG_FATAL_IF(gJavaBridge_ObjectID == NULL, "Unable to find android/webkit/JWebCoreJavaBridge.mNativeBridge");
    env->DeleteLocalRef(javaBridge);

    return jniRegisterNativeMethods(env, "android/webkit/JWebCoreJavaBridge", 
                                    gWebCoreJavaBridgeMethods, NELEM(gWebCoreJavaBridgeMethods));
}

} /* namespace android */
