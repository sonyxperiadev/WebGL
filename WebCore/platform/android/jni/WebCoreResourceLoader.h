/* //device/libs/android_runtime/android_webcore_resource_loader.h
**
** Copyright 2006, The Android Open Source Project
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

#ifndef ANDROID_WEBKIT_RESOURCELOADLISTENER_H
#define ANDROID_WEBKIT_RESOURCELOADLISTENER_H

#include "KURL.h"

#include "WebCoreRefObject.h"
#include <jni.h>

namespace android {

class WebCoreResourceLoader : public WebCoreRefObject
{
public:
    WebCoreResourceLoader(JNIEnv *env, jobject jLoadListener);
    virtual ~WebCoreResourceLoader();

    /**
     * Call to java to cancel the current load.
     */
    void cancel();

    /**
    * Call to java to download the current load rather than feed it
    * back to WebCore
    */
    void downloadFile();

    /**
    * Call to java to find out if this URL is in the cache
    */
    static bool willLoadFromCache(const WebCore::KURL& url);

    // Native jni functions
    static void SetResponseHeader(JNIEnv*, jobject, jint, jstring, jstring);
    static jint CreateResponse(JNIEnv*, jobject, jstring, jint, jstring,
            jstring, jlong, jstring, jlong);
    static void ReceivedResponse(JNIEnv*, jobject, jint);
    static void AddData(JNIEnv*, jobject, jbyteArray, jint);
    static void Finished(JNIEnv*, jobject);
    static jstring RedirectedToUrl(JNIEnv*, jobject, jstring, jstring, jint);
    static void Error(JNIEnv*, jobject, jint, jstring, jstring);

private:
    JavaVM*     mJvm;
    jobject     mJLoader;
};

} // end namespace android

#endif
