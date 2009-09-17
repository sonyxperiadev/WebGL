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
extern int register_webcorejni(JNIEnv*);
#if ENABLE(DATABASE)
extern int register_webstorage(JNIEnv*);
#endif
extern int register_geolocation_permissions(JNIEnv*);
extern int register_mock_geolocation(JNIEnv*);
#if ENABLE(VIDEO)
extern int register_mediaplayer(JNIEnv*);
#endif

}

struct RegistrationMethod {
    const char* name;
    int (*func)(JNIEnv*);
};

static RegistrationMethod gWebCoreRegMethods[] = {
    { "JavaBridge", android::register_javabridge },
    { "WebFrame", android::register_webframe },
    { "WebCoreResourceLoader", android::register_resource_loader },
    { "WebCoreJni", android::register_webcorejni },
    { "WebViewCore", android::register_webviewcore },
    { "WebHistory", android::register_webhistory },
    { "WebIconDatabase", android::register_webicondatabase },
    { "WebSettings", android::register_websettings },
#if ENABLE(DATABASE)
    { "WebStorage", android::register_webstorage },
#endif
    { "WebView", android::register_webview },
    { "GeolocationPermissions", android::register_geolocation_permissions },
    { "MockGeolocation", android::register_mock_geolocation },
#if ENABLE(VIDEO)
    { "HTML5VideoViewProxy", android::register_mediaplayer },
#endif
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
