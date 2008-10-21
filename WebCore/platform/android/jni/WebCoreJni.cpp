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

#include "WebCoreJni.h"
#include <jni.h>

#ifdef LOG
#undef LOG
#endif
#define LOG_TAG "webcoreglue"
#include <utils/Log.h>

namespace android {

JavaVM* WebCoreJni::mJavaVM;

extern int register_webcoreframebridge(JNIEnv*);
extern int register_javabridge(JNIEnv*);
extern int register_resource_loader(JNIEnv*);
extern int register_webviewcore(JNIEnv*);
extern int register_webhistory(JNIEnv*);
extern int register_webicondatabase(JNIEnv*);
extern int register_websettings(JNIEnv*);
extern int register_webview(JNIEnv*);

}

struct RegistrationMethod {
    const char* name;
    int (*func)(JNIEnv*);
};

static RegistrationMethod gWebCoreRegMethods[] = {
    { "JavaBridge", android::register_javabridge },
    { "WebCoreFrameBridge", android::register_webcoreframebridge },
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
    android::WebCoreJni::setJavaVM(vm);

    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        LOGE("GetEnv failed!");
        return result;
    }
    LOG_ASSERT(env, "Could not retrieve the env!");

    const RegistrationMethod* method = gWebCoreRegMethods;
    const RegistrationMethod* end = method + sizeof(gWebCoreRegMethods)/sizeof(RegistrationMethod);
    while (method != end) {
        if (method->func(env) < 0) {
            LOGE("%s registration failed!", method->name);
            return result;
        }
        method++;
    }

    return JNI_VERSION_1_4;
}
