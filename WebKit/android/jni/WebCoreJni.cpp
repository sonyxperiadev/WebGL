/* 
**
** Copyright 2007, The Android Open Source Project
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

#include "config.h"

#include "WebCoreJni.h"
#include "jni_utility.h"
#include <jni.h>
#include <utils/Log.h>

namespace android {

extern int register_webframe(JNIEnv*);
extern int register_javabridge(JNIEnv*);
extern int register_resource_loader(JNIEnv*);
extern int register_webviewcore(JNIEnv*);
extern int register_webhistory(JNIEnv*);
extern int register_webicondatabase(JNIEnv*);
extern int register_websettings(JNIEnv*);
extern int register_webview(JNIEnv*);

// Class, constructor, and get method on WeakReference
jclass    gWeakRefClass;
jmethodID gWeakRefInit;
jmethodID gWeakRefGet;

jobject adoptGlobalRef(JNIEnv* env, jobject obj)
{
    // Create a WeakReference object
    jobject ref = env->NewObject(gWeakRefClass, gWeakRefInit, obj);
    // Increment the ref count of the WeakReference
    ref = env->NewGlobalRef(ref);
    return ref;
}

AutoJObject getRealObject(JNIEnv* env, jobject obj)
{
    jobject real = env->CallObjectMethod(obj, gWeakRefGet);
    if (!real)
        LOGE("The real object has been deleted");
    return AutoJObject(env, real);
}

/**
 * Helper method for checking java exceptions
 * @return true if an exception occurred.
 */
bool checkException(JNIEnv* env)
{
    if (env->ExceptionCheck() != 0)
    {
        LOGE("*** Uncaught exception returned from Java call!\n");
        env->ExceptionDescribe();
        return true;
    }
    return false;
}

// This method is safe to call from the ui thread and the WebCore thread.
WebCore::String to_string(JNIEnv* env, jstring str)
{
    if (!str || !env)
        return WebCore::String();
    const jchar* s = env->GetStringChars(str, NULL);
    if (!s)
        return WebCore::String();
    WebCore::String ret(s, env->GetStringLength(str));
    env->ReleaseStringChars(str, s);
    checkException(env);
    return ret;
}

JavaVM* jnienv_to_javavm(JNIEnv* env)
{
    JavaVM* vm;
    return env->GetJavaVM(&vm) >= 0 ? vm : NULL;
}

JNIEnv* javavm_to_jnienv(JavaVM* vm)
{
    JNIEnv* env;
    return vm->GetEnv((void **)&env, JNI_VERSION_1_4) >= 0 ? env : NULL;
}

}

struct RegistrationMethod {
    const char* name;
    int (*func)(JNIEnv*);
};

static RegistrationMethod gWebCoreRegMethods[] = {
    { "JavaBridge", android::register_javabridge },
    { "WebFrame", android::register_webframe },
    { "WebCoreResourceLoader", android::register_resource_loader },
    { "WebViewCore", android::register_webviewcore },
    { "WebHistory", android::register_webhistory },
    { "WebIconDatabase", android::register_webicondatabase },
    { "WebSettings", android::register_websettings },
    { "WebView", android::register_webview }
};

EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    // Save the JavaVM pointer for use globally.
    JSC::Bindings::setJavaVM(vm);

    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        LOGE("GetEnv failed!");
        return result;
    }
    LOG_ASSERT(env, "Could not retrieve the env!");

    // Instantiate the WeakReference fields.
    android::gWeakRefClass = env->FindClass("java/lang/ref/WeakReference");
    LOG_ASSERT(android::gWeakRefClass, "Could not find WeakReference");
    android::gWeakRefInit = env->GetMethodID(android::gWeakRefClass,
            "<init>", "(Ljava/lang/Object;)V");
    LOG_ASSERT(android::gWeakRefInit,
            "Could not find constructor for WeakReference");
    android::gWeakRefGet = env->GetMethodID(android::gWeakRefClass, "get",
            "()Ljava/lang/Object;");
    LOG_ASSERT(android::gWeakRefInit,
            "Could not find get method for WeakReference");

    const RegistrationMethod* method = gWebCoreRegMethods;
    const RegistrationMethod* end = method + sizeof(gWebCoreRegMethods)/sizeof(RegistrationMethod);
    while (method != end) {
        if (method->func(env) < 0) {
            LOGE("%s registration failed!", method->name);
            return result;
        }
        method++;
    }
    
    // Initialize rand() function. The rand() function is used in
    // FileSystemAndroid to create a random temporary filename.
    srand(time(NULL));

    return JNI_VERSION_1_4;
}
