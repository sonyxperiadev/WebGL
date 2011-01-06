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

#define LOG_TAG "WebCore"

#include "config.h"
#include "MIMETypeRegistry.h"

#include "PlatformString.h"
#include "WebCoreJni.h"

#include <JNIUtility.h>
#include <jni.h>
#include <utils/Log.h>

using namespace android;

namespace WebCore {

String MIMETypeRegistry::getMIMETypeForExtension(const String& ext)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jclass mimeClass = env->FindClass("android/webkit/MimeTypeMap");
    LOG_ASSERT(mimeClass, "Could not find class MimeTypeMap");
    jmethodID mimeTypeFromExtension = env->GetStaticMethodID(mimeClass,
            "mimeTypeFromExtension",
            "(Ljava/lang/String;)Ljava/lang/String;");
    LOG_ASSERT(mimeTypeFromExtension,
            "Could not find method mimeTypeFromExtension");
    jstring extString = wtfStringToJstring(env, ext);
    jobject mimeType = env->CallStaticObjectMethod(mimeClass,
            mimeTypeFromExtension, extString);
    String result = android::jstringToWtfString(env, (jstring) mimeType);
    env->DeleteLocalRef(mimeClass);
    env->DeleteLocalRef(extString);
    env->DeleteLocalRef(mimeType);
    return result;
}

bool MIMETypeRegistry::isApplicationPluginMIMEType(const String&)
{
    return false;
}

}
