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
#include "MediaListener.h"

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

MediaTexture::MediaTexture(jobject weakWebViewRef) : android::LightRefBase<MediaTexture>()
{
    m_weakWebViewRef = weakWebViewRef;
    m_textureId = 0;
    m_dimensions.setEmpty();
    m_newWindowRequest = false;
    m_newWindowReady = false;
    m_mediaListener = new MediaListener(m_weakWebViewRef);
}

MediaTexture::~MediaTexture()
{
    releaseNativeWindow();
    if (m_textureId)
        glDeleteTextures(1, &m_textureId);
    if (m_weakWebViewRef) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        env->DeleteWeakGlobalRef(m_weakWebViewRef);
    }
}

void MediaTexture::initNativeWindowIfNeeded()
{
    {
        android::Mutex::Autolock lock(m_mediaLock);

        if(!m_newWindowRequest)
            return;

        // reuse an existing texture if possible
        if (!m_textureId)
            glGenTextures(1, &m_textureId);

        m_surfaceTexture = new android::SurfaceTexture(m_textureId);
        m_surfaceTextureClient = new android::SurfaceTextureClient(m_surfaceTexture);

        //setup callback
        m_mediaListener->resetFrameAvailable();
        m_surfaceTexture->setFrameAvailableListener(m_mediaListener);

        m_newWindowRequest = false;
        m_newWindowReady = true;
    }
    m_newMediaRequestCond.signal();
}

void MediaTexture::drawContent(const TransformationMatrix& matrix)
{
    android::Mutex::Autolock lock(m_mediaLock);

    if(!m_surfaceTexture.get() || m_dimensions.isEmpty()
            || !m_mediaListener->isFrameAvailable())
        return;

    m_surfaceTexture->updateTexImage();

    sp<GraphicBuffer> buf = m_surfaceTexture->getCurrentBuffer();
    PixelFormat f = buf->getPixelFormat();

    // only attempt to use alpha blending if alpha channel exists
    bool forceAlphaBlending = !(
        PIXEL_FORMAT_RGBX_8888 == f ||
        PIXEL_FORMAT_RGB_888 == f ||
        PIXEL_FORMAT_RGB_565 == f ||
        PIXEL_FORMAT_RGB_332 == f);

    TilesManager::instance()->shader()->drawLayerQuad(matrix, m_dimensions,
                                                      m_textureId, 1.0f,
                                                      forceAlphaBlending, GL_TEXTURE_EXTERNAL_OES);
}

void MediaTexture::drawVideo(const TransformationMatrix& matrix, const SkRect& parentBounds)
{
    android::Mutex::Autolock lock(m_mediaLock);

    if(!m_surfaceTexture.get() || m_dimensions.isEmpty()
            || !m_mediaListener->isFrameAvailable())
        return;

    m_surfaceTexture->updateTexImage();

    float surfaceMatrix[16];
    m_surfaceTexture->getTransformMatrix(surfaceMatrix);

    SkRect dimensions = m_dimensions;
    dimensions.offset(parentBounds.fLeft, parentBounds.fTop);

#ifdef DEBUG
    if (!parentBounds.contains(dimensions)) {
        XLOG("The video exceeds is parent's bounds.");
    }
#endif // DEBUG

    TilesManager::instance()->shader()->drawVideoLayerQuad(matrix, surfaceMatrix,
            dimensions, m_textureId);
}

ANativeWindow* MediaTexture::requestNewWindow()
{
    android::Mutex::Autolock lock(m_mediaLock);

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
    bool timedOut = false;
    while (m_newWindowRequest && !timedOut) {
        int ret = m_newMediaRequestCond.waitRelative(m_mediaLock, 500000000); // .5 sec
        timedOut = ret == TIMED_OUT;
    }

    if (m_surfaceTextureClient.get())
        m_newWindowReady = false;

    return m_surfaceTextureClient.get();
}

ANativeWindow* MediaTexture::getNativeWindow()
{
    android::Mutex::Autolock lock(m_mediaLock);
    return m_surfaceTextureClient.get();
}

void MediaTexture::releaseNativeWindow()
{
    android::Mutex::Autolock lock(m_mediaLock);
    m_dimensions.setEmpty();

    if (m_surfaceTexture.get())
        m_surfaceTexture->setFrameAvailableListener(0);

    // clear the strong pointer references
    m_surfaceTextureClient.clear();
    m_surfaceTexture.clear();
}

void MediaTexture::setDimensions(const SkRect& dimensions)
{
    android::Mutex::Autolock lock(m_mediaLock);
    m_dimensions = dimensions;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
