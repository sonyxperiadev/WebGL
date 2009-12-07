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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "webcoreglue"

#include <config.h>
#include <wtf/Platform.h>

#include "Cache.h"
#include "CookieClient.h"
#include "JavaSharedClient.h"
#include "KeyGeneratorClient.h"
#include "KURL.h"
#include "NetworkStateNotifier.h"
#include "Page.h"
#include "PluginClient.h"
#include "PluginDatabase.h"
#include "Timer.h"
#include "TimerClient.h"
#include "jni_utility.h"
#include "WebCoreJni.h"

#ifdef ANDROID_INSTRUMENT
#include "TimeCounter.h"
#endif

#include <jni.h>
#include <JNIHelp.h>
#include <SkUtils.h>
#include <utils/misc.h>

namespace android {

// ----------------------------------------------------------------------------

static jfieldID gJavaBridge_ObjectID;

// ----------------------------------------------------------------------------
   
class JavaBridge : public TimerClient, public CookieClient, public PluginClient, public KeyGeneratorClient
{
public:
    JavaBridge(JNIEnv* env, jobject obj);
    virtual ~JavaBridge();

    /*
     * WebCore -> Java API
     */
    virtual void setSharedTimer(long long timemillis);
    virtual void stopSharedTimer();

    virtual void setCookies(WebCore::KURL const& url, WebCore::String const& value);
    virtual WebCore::String cookies(WebCore::KURL const& url);
    virtual bool cookiesEnabled();

    virtual WTF::Vector<WebCore::String> getPluginDirectories();
    virtual WebCore::String getPluginSharedDataDirectory();

    virtual WTF::Vector<String> getSupportedKeyStrengthList();
    virtual WebCore::String getSignedPublicKeyAndChallengeString(unsigned index,
            const WebCore::String& challenge, const WebCore::KURL& url);

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
    static void SetDeferringTimers(JNIEnv* env, jobject obj, jboolean defer);
    static void ServiceFuncPtrQueue(JNIEnv*);
    static void UpdatePluginDirectories(JNIEnv* env, jobject obj, jobjectArray array, jboolean reload);

private:
    jobject     mJavaObject;
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
};

static void (*sSharedTimerFiredCallback)();

JavaBridge::JavaBridge(JNIEnv* env, jobject obj)
{
    mJavaObject = adoptGlobalRef(env, obj);
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
}

JavaBridge::~JavaBridge()
{
    if (mJavaObject) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        env->DeleteGlobalRef(mJavaObject);
        mJavaObject = 0;
    }
    
    JavaSharedClient::SetTimerClient(NULL);
    JavaSharedClient::SetCookieClient(NULL);
    JavaSharedClient::SetPluginClient(NULL);
    JavaSharedClient::SetKeyGeneratorClient(NULL);
}

void
JavaBridge::setSharedTimer(long long timemillis)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = getRealObject(env, mJavaObject);
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), mSetSharedTimer, timemillis);
}

void
JavaBridge::stopSharedTimer()
{    
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = getRealObject(env, mJavaObject);
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), mStopSharedTimer);
}

void
JavaBridge::setCookies(WebCore::KURL const& url, WebCore::String const& value)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = getRealObject(env, mJavaObject);
    if (!obj.get())
        return;
    const WebCore::String& urlStr = url.string();
    jstring jUrlStr = env->NewString(urlStr.characters(), urlStr.length());
    jstring jValueStr = env->NewString(value.characters(), value.length());

    env->CallVoidMethod(obj.get(), mSetCookies, jUrlStr, jValueStr);
    env->DeleteLocalRef(jUrlStr);
    env->DeleteLocalRef(jValueStr);
}

WebCore::String
JavaBridge::cookies(WebCore::KURL const& url)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    const WebCore::String& urlStr = url.string();
    AutoJObject obj = getRealObject(env, mJavaObject);
    if (!obj.get())
        return WebCore::String();
    jstring jUrlStr = env->NewString(urlStr.characters(), urlStr.length());
    jstring string = (jstring)(env->CallObjectMethod(obj.get(), mCookies, jUrlStr));
    
    WebCore::String ret = to_string(env, string);
    env->DeleteLocalRef(jUrlStr);
    env->DeleteLocalRef(string);
    return ret;
}

bool
JavaBridge::cookiesEnabled()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = getRealObject(env, mJavaObject);
    if (!obj.get())
        return false;
    jboolean ret = env->CallBooleanMethod(obj.get(), mCookiesEnabled);
    return (ret != 0);
}

WTF::Vector<WebCore::String>
JavaBridge::getPluginDirectories()
{
    WTF::Vector<WebCore::String> directories;
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = getRealObject(env, mJavaObject);
    if (!obj.get())
        return directories;
    jobjectArray array = (jobjectArray)
            env->CallObjectMethod(obj.get(), mGetPluginDirectories);
    int count = env->GetArrayLength(array);
    for (int i = 0; i < count; i++) {
        jstring dir = (jstring) env->GetObjectArrayElement(array, i);
        directories.append(to_string(env, dir));
        env->DeleteLocalRef(dir);
    }
    env->DeleteLocalRef(array);
    checkException(env);
    return directories;
}

WebCore::String
JavaBridge::getPluginSharedDataDirectory()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = getRealObject(env, mJavaObject);
    if (!obj.get())
        return WebCore::String();
    jstring ret = (jstring)env->CallObjectMethod(obj.get(), mGetPluginSharedDataDirectory);
    WebCore::String path = to_string(env, ret);
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
    if (!obj.get())
        return;
    env->CallVoidMethod(obj.get(), mSignalFuncPtrQueue);
}

WTF::Vector<WebCore::String>JavaBridge::getSupportedKeyStrengthList() {
    WTF::Vector<WebCore::String> list;
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject obj = getRealObject(env, mJavaObject);
    if (!obj.get())
        return list;
    jobjectArray array = (jobjectArray) env->CallObjectMethod(obj.get(),
            mGetKeyStrengthList);
    int count = env->GetArrayLength(array);
    for (int i = 0; i < count; ++i) {
        jstring keyStrength = (jstring) env->GetObjectArrayElement(array, i);
        list.append(to_string(env, keyStrength));
        env->DeleteLocalRef(keyStrength);
    }
    env->DeleteLocalRef(array);
    checkException(env);
    return list;
}

WebCore::String JavaBridge::getSignedPublicKeyAndChallengeString(unsigned index,
        const WebCore::String& challenge, const WebCore::KURL& url) {
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jstring jChallenge = env->NewString(challenge.characters(),
            challenge.length());
    const WebCore::String& urlStr = url.string();
    jstring jUrl = env->NewString(urlStr.characters(), urlStr.length());
    AutoJObject obj = getRealObject(env, mJavaObject);
    jstring key = (jstring) env->CallObjectMethod(obj.get(),
            mGetSignedPublicKey, index, jChallenge, jUrl);
    WebCore::String ret = to_string(env, key);
    env->DeleteLocalRef(jChallenge);
    env->DeleteLocalRef(jUrl);
    env->DeleteLocalRef(key);
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

void JavaBridge::ServiceFuncPtrQueue(JNIEnv*)
{
    JavaSharedClient::ServiceFunctionPtrQueue();
}

void JavaBridge::UpdatePluginDirectories(JNIEnv* env, jobject obj,
        jobjectArray array, jboolean reload) {
    WTF::Vector<WebCore::String> directories;
    int count = env->GetArrayLength(array);
    for (int i = 0; i < count; i++) {
        jstring dir = (jstring) env->GetObjectArrayElement(array, i);
        directories.append(to_string(env, dir));
        env->DeleteLocalRef(dir);
    }
    checkException(env);
    WebCore::PluginDatabase *pluginDatabase =
            WebCore::PluginDatabase::installedPlugins();
    pluginDatabase->setPluginDirectories(directories);
    // refreshPlugins() should refresh both PluginDatabase and Page's PluginData
    WebCore::Page::refreshPlugins(reload);
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
    { "nativeServiceFuncPtrQueue", "()V",
        (void*) JavaBridge::ServiceFuncPtrQueue },
    { "nativeUpdatePluginDirectories", "([Ljava/lang/String;Z)V",
        (void*) JavaBridge::UpdatePluginDirectories }
};

int register_javabridge(JNIEnv* env)
{
    jclass javaBridge = env->FindClass("android/webkit/JWebCoreJavaBridge");
    LOG_FATAL_IF(javaBridge == NULL, "Unable to find class android/webkit/JWebCoreJavaBridge");
    gJavaBridge_ObjectID = env->GetFieldID(javaBridge, "mNativeBridge", "I");
    LOG_FATAL_IF(gJavaBridge_ObjectID == NULL, "Unable to find android/webkit/JWebCoreJavaBridge.mNativeBridge");

    return jniRegisterNativeMethods(env, "android/webkit/JWebCoreJavaBridge", 
                                    gWebCoreJavaBridgeMethods, NELEM(gWebCoreJavaBridgeMethods));
}

} /* namespace android */
