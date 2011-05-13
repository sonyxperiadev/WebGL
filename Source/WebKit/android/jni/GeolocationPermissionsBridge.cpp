/*
 * Copyright 2009, The Android Open Source Project
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

#include <JNIHelp.h>  // For jniRegisterNativeMethods
#include "GeolocationPermissions.h"
#include "WebCoreJni.h"  // For jstringToWtfString


/**
 * This file provides a set of functions to bridge between the Java and C++
 * GeolocationPermissions classes. The java GeolocationPermissions object calls
 * the functions provided here, which in turn call static methods on the C++
 * GeolocationPermissions class.
 */

namespace android {

static jobject getOrigins(JNIEnv* env, jobject obj)
{
    GeolocationPermissions::OriginSet origins = GeolocationPermissions::getOrigins();

    jclass setClass = env->FindClass("java/util/HashSet");
    jmethodID constructor = env->GetMethodID(setClass, "<init>", "()V");
    jmethodID addMethod = env->GetMethodID(setClass, "add", "(Ljava/lang/Object;)Z");
    jobject set = env->NewObject(setClass, constructor);
    env->DeleteLocalRef(setClass);

    GeolocationPermissions::OriginSet::const_iterator end = origins.end();
    for (GeolocationPermissions::OriginSet::const_iterator iter = origins.begin(); iter != end; ++iter) {
        jstring originString = wtfStringToJstring(env, *iter);
        env->CallBooleanMethod(set, addMethod, originString);
        env->DeleteLocalRef(originString);
    }
    return set;
}

static bool getAllowed(JNIEnv* env, jobject obj, jstring origin)
{
    WTF::String originString = jstringToWtfString(env, origin);
    return GeolocationPermissions::getAllowed(originString);
}

static void clear(JNIEnv* env, jobject obj, jstring origin)
{
    WTF::String originString = jstringToWtfString(env, origin);
    GeolocationPermissions::clear(originString);
}

static void allow(JNIEnv* env, jobject obj, jstring origin)
{
    WTF::String originString = jstringToWtfString(env, origin);
    GeolocationPermissions::allow(originString);
}

static void clearAll(JNIEnv* env, jobject obj)
{
    GeolocationPermissions::clearAll();
}

/*
 * JNI registration
 */
static JNINativeMethod gGeolocationPermissionsMethods[] = {
    { "nativeGetOrigins", "()Ljava/util/Set;",
        (void*) getOrigins },
    { "nativeGetAllowed", "(Ljava/lang/String;)Z",
        (void*) getAllowed },
    { "nativeClear", "(Ljava/lang/String;)V",
        (void*) clear },
    { "nativeAllow", "(Ljava/lang/String;)V",
        (void*) allow },
    { "nativeClearAll", "()V",
        (void*) clearAll }
};

int registerGeolocationPermissions(JNIEnv* env)
{
    const char* kGeolocationPermissionsClass = "android/webkit/GeolocationPermissions";
#ifndef NDEBUG
    jclass geolocationPermissions = env->FindClass(kGeolocationPermissionsClass);
    LOG_ASSERT(geolocationPermissions, "Unable to find class");
    env->DeleteLocalRef(geolocationPermissions);
#endif

    return jniRegisterNativeMethods(env, kGeolocationPermissionsClass,
            gGeolocationPermissionsMethods, NELEM(gGeolocationPermissionsMethods));
}

}
