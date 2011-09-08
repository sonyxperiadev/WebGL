/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MediaListener_h
#define MediaListener_h

#if USE(ACCELERATED_COMPOSITING)

#include <gui/SurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>
#include <jni.h>
#include <JNIUtility.h>
#include "MediaTexture.h"
#include "WebCoreJni.h"

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "MediaListener", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

class MediaListener : public android::SurfaceTexture::FrameAvailableListener {

public:
    MediaListener(jobject weakWebViewRef,
                  const sp<android::SurfaceTexture>& surfaceTexture,
                  const sp<ANativeWindow>& nativeWindow)
        : m_weakWebViewRef(weakWebViewRef)
        , m_postInvalMethod(0)
        , m_frameAvailable(false)
        , m_surfaceTexture(surfaceTexture)
        , m_nativeWindow(nativeWindow)
        , m_framerateCallback(0)
    {
        if (!m_weakWebViewRef)
            return;

        JNIEnv* env = JSC::Bindings::getJNIEnv();
        jobject localWebViewRef = env->NewLocalRef(m_weakWebViewRef);
        if (localWebViewRef) {
            jclass wvClass = env->GetObjectClass(localWebViewRef);
            m_postInvalMethod = env->GetMethodID(wvClass, "postInvalidate", "()V");
            env->DeleteLocalRef(wvClass);
            env->DeleteLocalRef(localWebViewRef);
        }
        checkException(env);
    }

    virtual void onFrameAvailable()
    {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        jobject localWebViewRef = env->NewLocalRef(m_weakWebViewRef);
        if (localWebViewRef) {
            env->CallVoidMethod(localWebViewRef, m_postInvalMethod);
            env->DeleteLocalRef(localWebViewRef);
        }
        checkException(env);
        if (!m_frameAvailable) {
            m_frameAvailable = true;
        }
        if (m_framerateCallback)
            m_framerateCallback(m_nativeWindow.get(), m_surfaceTexture->getTimestamp());
    }

    bool isFrameAvailable() { return m_frameAvailable; }
    void setFramerateCallback(FramerateCallbackProc callback) { m_framerateCallback = callback; }

private:
    jobject m_weakWebViewRef;
    jmethodID m_postInvalMethod;
    bool m_frameAvailable;
    sp<android::SurfaceTexture> m_surfaceTexture;
    sp<ANativeWindow> m_nativeWindow;
    FramerateCallbackProc m_framerateCallback;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif  // MediaListener_h
