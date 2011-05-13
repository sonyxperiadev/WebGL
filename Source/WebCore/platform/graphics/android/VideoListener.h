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

#ifndef VideoListener_h
#define VideoListener_h

#if USE(ACCELERATED_COMPOSITING)

#include <gui/SurfaceTexture.h>
#include <jni.h>
#include <JNIUtility.h>
#include "WebCoreJni.h"

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "VideoListener", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

class VideoListener : public android::SurfaceTexture::FrameAvailableListener {

public:
    VideoListener(jobject weakWebViewRef)
        : m_weakWebViewRef(weakWebViewRef)
        , m_postInvalMethod(0)
        , m_frameAvailable(false)
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
    }

    void resetFrameAvailable() { m_frameAvailable = false; }
    bool isFrameAvailable() { return m_frameAvailable; }

private:
    jobject m_weakWebViewRef;
    jmethodID m_postInvalMethod;
    bool m_frameAvailable;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif  // VideoListener_h
