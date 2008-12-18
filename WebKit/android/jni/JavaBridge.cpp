/*
** Copyright 2006-2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#define LOG_TAG "webcoreglue"

#include <config.h>
#include <wtf/Platform.h>

#include "Cache.h"
#include "CookieClient.h"
#include "JavaSharedClient.h"
#include "KURL.h"
#include "NetworkStateNotifier.h"
#include "Timer.h"
#include "TimerClient.h"
#include "jni_utility.h"
#include "WebCoreJni.h"

#ifdef ANDROID_INSTRUMENT
#include "Frame.h"
#include "SystemTime.h"
#endif

#include <jni.h>
#include <JNIHelp.h>
#include <SkImageRef_GlobalPool.h>
#include <SkUtils.h>
#include <utils/misc.h>

// maximum bytes used to cache decoded images
// (not including big images using ashmem)
#define IMAGE_POOL_BUDGET   (512 * 1024)

#ifdef ANDROID_INSTRUMENT
static uint32_t sTotalTimeUsed = 0;

namespace WebCore {
void Frame::resetSharedTimerTimeCounter()
{
    sTotalTimeUsed = 0;
}

void Frame::reportSharedTimerTimeCounter()
{
    LOG(LOG_DEBUG, "WebCore", "*-* Total native 2 (shared timer) time: %d ms\n", 
            sTotalTimeUsed);   
}
}
#endif

namespace android {

// ----------------------------------------------------------------------------

static jfieldID gJavaBridge_ObjectID;

// ----------------------------------------------------------------------------
   
class JavaBridge : public WebCore::TimerClient, public WebCore::CookieClient
{
public:
    JavaBridge(JNIEnv* env, jobject obj);
    virtual ~JavaBridge();

    /*
     * WebCore -> Java API
     */
    virtual void setSharedTimer(long long timemillis);
    virtual void stopSharedTimer();

    virtual void setCookies(WebCore::KURL const& url, WebCore::KURL const& docURL, WebCore::String const& value);
    virtual WebCore::String cookies(WebCore::KURL const& url);
    virtual bool cookiesEnabled();

    ////////////////////////////////////////////

    virtual void setSharedTimerCallback(void (*f)());

    ////////////////////////////////////////////

    void signalServiceFuncPtrQueue();

    // jni functions
    static void Constructor(JNIEnv* env, jobject obj);
    static void Finalize(JNIEnv* env, jobject obj);
    static void SharedTimerFired(JNIEnv* env, jobject);
    static void SetCacheSize(JNIEnv* env, jobject obj, jint bytes);
    static void SetNetworkOnLine(JNIEnv* env, jobject obj, jboolean online);
    static void SetDeferringTimers(JNIEnv* env, jobject obj, jboolean defer);
    static void ServiceFuncPtrQueue(JNIEnv*);

private:
    JavaVM*     mJvm;
    jobject     mJavaObject;
    jmethodID   mSetSharedTimer;
    jmethodID   mStopSharedTimer;
    jmethodID   mSetCookies;
    jmethodID   mCookies;
    jmethodID   mCookiesEnabled;
    jmethodID   mSignalFuncPtrQueue;
};

static void (*sSharedTimerFiredCallback)();
static JavaBridge* gJavaBridge;

JavaBridge::JavaBridge(JNIEnv* env, jobject obj)
{
    mJvm = jnienv_to_javavm(env);
    mJavaObject = adoptGlobalRef(env, obj);
    jclass clazz = env->GetObjectClass(obj);

    mSetSharedTimer = env->GetMethodID(clazz, "setSharedTimer", "(J)V");
    mStopSharedTimer = env->GetMethodID(clazz, "stopSharedTimer", "()V");
    mSetCookies = env->GetMethodID(clazz, "setCookies", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    mCookies = env->GetMethodID(clazz, "cookies", "(Ljava/lang/String;)Ljava/lang/String;");
    mCookiesEnabled = env->GetMethodID(clazz, "cookiesEnabled", "()Z");
    mSignalFuncPtrQueue = env->GetMethodID(clazz, "signalServiceFuncPtrQueue", "()V");

    LOG_ASSERT(mSetSharedTimer, "Could not find method setSharedTimer");
    LOG_ASSERT(mStopSharedTimer, "Could not find method stopSharedTimer");
    LOG_ASSERT(mSetCookies, "Could not find method setCookies");
    LOG_ASSERT(mCookies, "Could not find method cookies");
    LOG_ASSERT(mCookiesEnabled, "Could not find method cookiesEnabled");

    WebCore::JavaSharedClient::SetTimerClient(this);
    WebCore::JavaSharedClient::SetCookieClient(this);
    gJavaBridge = this;
}   
    
JavaBridge::~JavaBridge()
{
    if (mJavaObject) {
        JNIEnv* env = javavm_to_jnienv(mJvm);
        env->DeleteGlobalRef(mJavaObject);
        mJavaObject = 0;
    }
    
    WebCore::JavaSharedClient::SetTimerClient(NULL);
    WebCore::JavaSharedClient::SetCookieClient(NULL);
}

void
JavaBridge::setSharedTimer(long long timemillis)
{
    JNIEnv* env = javavm_to_jnienv(mJvm);
    AutoJObject obj = getRealObject(env, mJavaObject);
    env->CallVoidMethod(obj.get(), mSetSharedTimer, timemillis);
}

void
JavaBridge::stopSharedTimer()
{    
    JNIEnv* env = javavm_to_jnienv(mJvm);
    AutoJObject obj = getRealObject(env, mJavaObject);
    env->CallVoidMethod(obj.get(), mStopSharedTimer);
}

void
JavaBridge::setCookies(WebCore::KURL const& url, WebCore::KURL const& docUrl, WebCore::String const& value)
{
    JNIEnv* env = javavm_to_jnienv(mJvm);
    const WebCore::String& urlStr = url.string();
    jstring jUrlStr = env->NewString(urlStr.characters(), urlStr.length());
    const WebCore::String& docUrlStr = docUrl.string();
    jstring jDocUrlStr = env->NewString(docUrlStr.characters(), docUrlStr.length());
    jstring jValueStr = env->NewString(value.characters(), value.length());

    AutoJObject obj = getRealObject(env, mJavaObject);
    env->CallVoidMethod(obj.get(), mSetCookies, jUrlStr, jDocUrlStr, jValueStr);
    env->DeleteLocalRef(jUrlStr);
    env->DeleteLocalRef(jDocUrlStr);
    env->DeleteLocalRef(jValueStr);
}

WebCore::String
JavaBridge::cookies(WebCore::KURL const& url)
{
    JNIEnv* env = javavm_to_jnienv(mJvm);
    const WebCore::String& urlStr = url.string();
    jstring jUrlStr = env->NewString(urlStr.characters(), urlStr.length());

    AutoJObject obj = getRealObject(env, mJavaObject);
    jstring string = (jstring)(env->CallObjectMethod(obj.get(), mCookies, jUrlStr));
    
    WebCore::String ret = to_string(env, string);
    env->DeleteLocalRef(jUrlStr);
    env->DeleteLocalRef(string);
    return ret;
}

bool
JavaBridge::cookiesEnabled()
{
    JNIEnv* env = javavm_to_jnienv(mJvm);
    AutoJObject obj = getRealObject(env, mJavaObject);
    jboolean ret = env->CallBooleanMethod(obj.get(), mCookiesEnabled);
    return (ret != 0);
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
    JSC::Bindings::getJNIEnv();
    JNIEnv* env = javavm_to_jnienv(mJvm);
    AutoJObject obj = getRealObject(env, mJavaObject);
    env->CallVoidMethod(obj.get(), mSignalFuncPtrQueue);
}

// ----------------------------------------------------------------------------

// visible to Shared
void AndroidSignalServiceFuncPtrQueue()
{
    gJavaBridge->signalServiceFuncPtrQueue();
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
        uint32_t startTime = WebCore::get_thread_msec();
#endif
        SkAutoMemoryUsageProbe  mup("JavaBridge::sharedTimerFired");
        sSharedTimerFiredCallback();
#ifdef ANDROID_INSTRUMENT
        sTotalTimeUsed += WebCore::get_thread_msec() - startTime;
#endif
    }
}

void JavaBridge::SetCacheSize(JNIEnv* env, jobject obj, jint bytes)
{
    WebCore::cache()->setCapacities(0, bytes/2, bytes);
    SkImageRef_GlobalPool::SetRAMBudget(IMAGE_POOL_BUDGET);
    LOGV("--- set ImageRef budget %d\n", SkImageRef_GlobalPool::GetRAMBudget());
}

void JavaBridge::SetNetworkOnLine(JNIEnv* env, jobject obj, jboolean online)
{
	WebCore::networkStateNotifier().networkStateChange(online);
}

void JavaBridge::SetDeferringTimers(JNIEnv* env, jobject obj, jboolean defer)
{
    WebCore::setDeferringTimers(defer);
}

void JavaBridge::ServiceFuncPtrQueue(JNIEnv*)
{
    WebCore::JavaSharedClient::ServiceFunctionPtrQueue();
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
    { "setDeferringTimers", "(Z)V",
        (void*) JavaBridge::SetDeferringTimers },
    { "nativeServiceFuncPtrQueue", "()V",
        (void*) JavaBridge::ServiceFuncPtrQueue },
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
