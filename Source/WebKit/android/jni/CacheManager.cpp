/*
 * Copyright 2011, The Android Open Source Project
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

#if USE(CHROME_NETWORK_STACK)

#include "ChromiumIncludes.h"
#include "WebCache.h"
#include "WebCoreJni.h"

#include <JNIHelp.h>
#include <platform/FileSystem.h>
#include <platform/text/Base64.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;
using namespace base;
using namespace disk_cache;
using namespace net;
using namespace std;

namespace android {

// JNI for android.webkit.CacheManager
static const char* javaCacheManagerClass = "android/webkit/CacheManager";

static void setStringField(JNIEnv* env, const jobject& object, const jfieldID& field, const String& str)
{
    jstring jstr = wtfStringToJstring(env, str);
    env->SetObjectField(object, field, jstr);
    env->DeleteLocalRef(jstr);
}

static void setFieldFromHeaderIfPresent(CacheResult* result, const char* header, JNIEnv* env, const jobject& object, const jfieldID& field, bool allowEmptyString)
{
  String value;
  if (result->firstResponseHeader(header, &value, allowEmptyString))
      setStringField(env, object, field, value);
}

static String getCacheFileBaseDir(JNIEnv* env)
{
    static String baseDir;
    if (baseDir.isEmpty()) {
        jclass cacheManagerClass = env->FindClass("android/webkit/CacheManager");
        jmethodID getCacheFileBaseDirMethod = env->GetStaticMethodID(cacheManagerClass, "getCacheFileBaseDir", "()Ljava/io/File;");
        jclass fileClass = env->FindClass("java/io/File");
        jmethodID getPathMethod = env->GetMethodID(fileClass, "getPath", "()Ljava/lang/String;");
        jobject fileObject = env->CallStaticObjectMethod(cacheManagerClass, getCacheFileBaseDirMethod);
        baseDir = jstringToWtfString(env, static_cast<jstring>(env->CallObjectMethod(fileObject, getPathMethod)));
    }
    return baseDir;
}

static jobject getCacheResult(JNIEnv* env, jobject, jstring url)
{
    // This is called on the UI thread.
    scoped_refptr<CacheResult> result = WebCache::get(false /*privateBrowsing*/)->getCacheResult(jstringToWtfString(env, url));
    if (!result)
        return 0;

    // We create and populate a file with the cache entry. This allows us to
    // replicate the behaviour of the Android HTTP stack in the Java
    // CacheManager, which opens the cache file and provides an input stream to
    // the file as part of the Java CacheResult object!
    String urlWtfString = jstringToWtfString(env, url);
    Vector<char> encodedUrl;
    base64Encode(urlWtfString.utf8().data(), urlWtfString.length(), encodedUrl, false /*insertLFs*/);
    String filePath = pathByAppendingComponent(getCacheFileBaseDir(env), encodedUrl.data());
    if (!result->writeToFile(filePath))
        return 0;

    jclass cacheResultClass = env->FindClass("android/webkit/CacheManager$CacheResult");
    jmethodID constructor = env->GetMethodID(cacheResultClass, "<init>", "()V");
    // We only bother with the fields that are made accessible through the public API.
    jfieldID contentdispositionField = env->GetFieldID(cacheResultClass, "contentdisposition", "Ljava/lang/String;");
    jfieldID contentLengthField = env->GetFieldID(cacheResultClass, "contentLength", "J");
    jfieldID etagField = env->GetFieldID(cacheResultClass, "etag", "Ljava/lang/String;");
    jfieldID encodingField = env->GetFieldID(cacheResultClass, "encoding", "Ljava/lang/String;");
    jfieldID expiresField = env->GetFieldID(cacheResultClass, "expires", "J");
    jfieldID expiresStringField = env->GetFieldID(cacheResultClass, "expiresString", "Ljava/lang/String;");
    jfieldID httpStatusCodeField = env->GetFieldID(cacheResultClass, "httpStatusCode", "I");
    jfieldID lastModifiedField = env->GetFieldID(cacheResultClass, "lastModified", "Ljava/lang/String;");
    jfieldID localPathField = env->GetFieldID(cacheResultClass, "localPath", "Ljava/lang/String;");
    jfieldID locationField = env->GetFieldID(cacheResultClass, "location", "Ljava/lang/String;");
    jfieldID mimeTypeField = env->GetFieldID(cacheResultClass, "mimeType", "Ljava/lang/String;");

    jobject javaResult = env->NewObject(cacheResultClass, constructor);
    setFieldFromHeaderIfPresent(result.get(), "content-disposition", env, javaResult, contentdispositionField, true);
    env->SetLongField(javaResult, contentLengthField, result->contentSize());
    setFieldFromHeaderIfPresent(result.get(), "etag", env, javaResult, etagField, false);
    setStringField(env, javaResult, encodingField, "TODO"); // TODO: Where does the Android stack set this?
    env->SetLongField(javaResult, expiresField, result->expires());
    env->SetIntField(javaResult, httpStatusCodeField, result->responseCode());
    setFieldFromHeaderIfPresent(result.get(), "last-modified", env, javaResult, lastModifiedField, false);
    setStringField(env, javaResult, localPathField, encodedUrl.data());
    setFieldFromHeaderIfPresent(result.get(), "location", env, javaResult, locationField, false);
    setStringField(env, javaResult, mimeTypeField, result->mimeType());

    return javaResult;
}

static JNINativeMethod gCacheManagerMethods[] = {
    { "nativeGetCacheResult", "(Ljava/lang/String;)Landroid/webkit/CacheManager$CacheResult;", (void*) getCacheResult },
};

int registerCacheManager(JNIEnv* env)
{
#ifndef NDEBUG
    jclass cacheManager = env->FindClass(javaCacheManagerClass);
    LOG_ASSERT(cacheManager, "Unable to find class");
    env->DeleteLocalRef(cacheManager);
#endif
    return jniRegisterNativeMethods(env, javaCacheManagerClass, gCacheManagerMethods, NELEM(gCacheManagerMethods));
}

} // namespace android

#endif //  USE(CHROME_NETWORK_STACK)
