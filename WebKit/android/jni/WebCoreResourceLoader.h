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

#ifndef ANDROID_WEBKIT_RESOURCELOADLISTENER_H
#define ANDROID_WEBKIT_RESOURCELOADLISTENER_H

#include <KURL.h>
#include <ResourceLoaderAndroid.h>
#include <jni.h>

namespace android {

class WebCoreResourceLoader : public WebCore::ResourceLoaderAndroid
{
public:
    static PassRefPtr<WebCore::ResourceLoaderAndroid> create(JNIEnv *env, jobject jLoadListener);
    virtual ~WebCoreResourceLoader();

    /**
     * Call to java to cancel the current load.
     */
    virtual void cancel();

    /**
    * Call to java to download the current load rather than feed it
    * back to WebCore
    */
    virtual void downloadFile();

    virtual void pauseLoad(bool);

    /**
    * Call to java to find out if this URL is in the cache
    */
    static bool willLoadFromCache(const WebCore::KURL& url, int64_t identifier);

    // Native jni functions
    static void SetResponseHeader(JNIEnv*, jobject, jint, jstring, jstring);
    static jint CreateResponse(JNIEnv*, jobject, jstring, jint, jstring,
            jstring, jlong, jstring);
    static void ReceivedResponse(JNIEnv*, jobject, jint);
    static void AddData(JNIEnv*, jobject, jbyteArray, jint);
    static void Finished(JNIEnv*, jobject);
    static jstring RedirectedToUrl(JNIEnv*, jobject, jstring, jstring, jint);
    static void Error(JNIEnv*, jobject, jint, jstring, jstring);

protected:
    WebCoreResourceLoader(JNIEnv *env, jobject jLoadListener);
private:
    jobject     mJLoader;
    bool        mPausedLoad;
};

} // end namespace android

#endif
