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

#include "ChromiumIncludes.h"
#include <JNIHelp.h>

namespace android {

static const char* javaJniUtilClass = "android/webkit/JniUtil";

static bool useChromiumHttpStack(JNIEnv*, jobject)
{
#if USE(CHROME_NETWORK_STACK)
    return true;
#else
    return false;
#endif
}

static JNINativeMethod gJniUtilMethods[] = {
    { "nativeUseChromiumHttpStack", "()Z", (void*) useChromiumHttpStack },
};

int registerJniUtil(JNIEnv* env)
{
#ifndef NDEBUG
    jclass jniUtil = env->FindClass(javaJniUtilClass);
    LOG_ASSERT(jniUtil, "Unable to find class");
    env->DeleteLocalRef(jniUtil);
#endif
    return jniRegisterNativeMethods(env, javaJniUtilClass, gJniUtilMethods, NELEM(gJniUtilMethods));
}

} // namespace android
