/*
 * Copyright 2007, The Android Open Source Project
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

#include "config.h"

#include "NotImplemented.h"
#include "WebCoreJni.h"
#include "jni_utility.h"
#include <jni.h>
#include <utils/Log.h>

namespace android {

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

int register_webcorejni(JNIEnv* env) {
    // Instantiate the WeakReference fields.
    jclass weakRef = env->FindClass("java/lang/ref/WeakReference");
    LOG_ASSERT(weakRef, "Could not find WeakReference");
    android::gWeakRefClass = (jclass)env->NewGlobalRef(weakRef);
    android::gWeakRefInit = env->GetMethodID(android::gWeakRefClass,
            "<init>", "(Ljava/lang/Object;)V");
    LOG_ASSERT(android::gWeakRefInit,
            "Could not find constructor for WeakReference");
    android::gWeakRefGet = env->GetMethodID(android::gWeakRefClass, "get",
            "()Ljava/lang/Object;");
    LOG_ASSERT(android::gWeakRefInit,
            "Could not find get method for WeakReference");
    return JNI_OK;
}

}
