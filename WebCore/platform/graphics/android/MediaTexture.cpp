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
#include "config.h"
#include "MediaTexture.h"
#include "TilesManager.h"
#include "GLUtils.h"
#include "VideoListener.h"

#if USE(ACCELERATED_COMPOSITING)

#include <android/native_window.h>
#include <gui/SurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>
#include <wtf/CurrentTime.h>
#include <JNIUtility.h>
#include "WebCoreJni.h"

#define LAYER_DEBUG
#undef LAYER_DEBUG

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "MediaTexture", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

VideoTexture::VideoTexture(jobject weakWebViewRef) : android::LightRefBase<VideoTexture>()
{
    m_weakWebViewRef = weakWebViewRef;
    m_textureId = 0;
    m_dimensions.setEmpty();
    m_newWindowRequest = false;
    m_newWindowReady = false;
    m_videoListener = new VideoListener(m_weakWebViewRef);
}

VideoTexture::~VideoTexture()
{
    releaseNativeWindow();
    if (m_textureId)
        glDeleteTextures(1, &m_textureId);
    if (m_weakWebViewRef) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        env->DeleteWeakGlobalRef(m_weakWebViewRef);
    }
}

void VideoTexture::initNativeWindowIfNeeded()
{
    android::Mutex::Autolock lock(m_videoLock);

    if(!m_newWindowRequest)
        return;

    // reuse an existing texture if possible
    if (!m_textureId)
        glGenTextures(1, &m_textureId);

    m_surfaceTexture = new android::SurfaceTexture(m_textureId);
    m_surfaceTextureClient = new android::SurfaceTextureClient(m_surfaceTexture);

    //setup callback
    m_videoListener->resetFrameAvailable();
    m_surfaceTexture->setFrameAvailableListener(m_videoListener);

    m_newWindowRequest = false;
    m_newWindowReady = true;
    m_newVideoRequestCond.signal();
}

void VideoTexture::drawVideo(const TransformationMatrix& matrix)
{
    android::Mutex::Autolock lock(m_videoLock);

    if(!m_surfaceTexture.get() || m_dimensions.isEmpty()
            || !m_videoListener->isFrameAvailable())
        return;

    m_surfaceTexture->updateTexImage();

    float surfaceMatrix[16];
    m_surfaceTexture->getTransformMatrix(surfaceMatrix);

    TilesManager::instance()->shader()->drawVideoLayerQuad(matrix, surfaceMatrix,
            m_dimensions, m_textureId);
}

ANativeWindow* VideoTexture::requestNewWindow()
{
    android::Mutex::Autolock lock(m_videoLock);

    // the window was not ready before the timeout so return it this time
    if (m_newWindowReady) {
        m_newWindowReady = false;
        return m_surfaceTextureClient.get();
    }
    // we only allow for one texture, so if one already exists return null
    else if (m_surfaceTextureClient.get()) {
        return 0;
    }

    m_newWindowRequest = true;

    // post an inval message to the UI thread to fulfill the request
    if (m_weakWebViewRef) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        jobject localWebViewRef = env->NewLocalRef(m_weakWebViewRef);
        if (localWebViewRef) {
            jclass wvClass = env->GetObjectClass(localWebViewRef);
            jmethodID postInvalMethod = env->GetMethodID(wvClass, "postInvalidate", "()V");
            env->CallVoidMethod(localWebViewRef, postInvalMethod);
            env->DeleteLocalRef(wvClass);
            env->DeleteLocalRef(localWebViewRef);
        }
        checkException(env);
    }

    //block until the request can be fulfilled or we time out
    m_newVideoRequestCond.waitRelative(m_videoLock, 500000000); // .5 sec

    if (m_surfaceTextureClient.get())
        m_newWindowReady = false;

    return m_surfaceTextureClient.get();
}

ANativeWindow* VideoTexture::getNativeWindow()
{
    android::Mutex::Autolock lock(m_videoLock);
    return m_surfaceTextureClient.get();
}

void VideoTexture::releaseNativeWindow()
{
    android::Mutex::Autolock lock(m_videoLock);
    m_dimensions.setEmpty();

    if (m_surfaceTexture.get())
        m_surfaceTexture->setFrameAvailableListener(0);

    // clear the strong pointer references
    m_surfaceTextureClient.clear();
    m_surfaceTexture.clear();
}

void VideoTexture::setDimensions(const SkRect& dimensions)
{
    android::Mutex::Autolock lock(m_videoLock);
    m_dimensions = dimensions;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
