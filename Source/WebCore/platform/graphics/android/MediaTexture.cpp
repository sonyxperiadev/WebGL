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

// Limits the number of ANativeWindows that can be allocated for video playback.
// The limit is currently set to 2 as that is the current max number of
// simultaneous HW decodes that our OMX implementation allows.  This forces the
// media producer to use their own SW decoders for subsequent video streams.
#define MAX_WINDOW_COUNT 2

namespace WebCore {

MediaTexture::MediaTexture(jobject webViewRef) : android::LightRefBase<MediaTexture>()
{
    if (webViewRef) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        m_weakWebViewRef = env->NewWeakGlobalRef(webViewRef);
    } else {
        m_weakWebViewRef = 0;
    }

    m_contentTexture = 0;
    m_isContentInverted = false;
    m_newWindowRequest = false;
}

MediaTexture::~MediaTexture()
{
    if (m_contentTexture)
        deleteTexture(m_contentTexture, true);
    for (unsigned int i = 0; i < m_videoTextures.size(); i++) {
        deleteTexture(m_videoTextures[i], true);
    }

    if (m_weakWebViewRef) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        env->DeleteWeakGlobalRef(m_weakWebViewRef);
    }
}

bool MediaTexture::isContentInverted()
{
    android::Mutex::Autolock lock(m_mediaLock);
    return m_isContentInverted;
}
void MediaTexture::invertContents(bool invertContent)
{
    android::Mutex::Autolock lock(m_mediaLock);
    m_isContentInverted = invertContent;
}

void MediaTexture::initNativeWindowIfNeeded()
{
    {
        android::Mutex::Autolock lock(m_mediaLock);

        // check to see if there are any unused textures to delete
        if (m_unusedTextures.size() != 0) {
            for (unsigned int i = 0; i < m_unusedTextures.size(); i++) {
                glDeleteTextures(1, &m_unusedTextures[i]);
            }
            m_unusedTextures.clear();
        }

        // create a content texture if none exists
        if (!m_contentTexture) {
            m_contentTexture = createTexture();

            // send a message to the WebKit thread to notify the plugin that it can draw
            if (m_weakWebViewRef) {
                JNIEnv* env = JSC::Bindings::getJNIEnv();
                jobject localWebViewRef = env->NewLocalRef(m_weakWebViewRef);
                if (localWebViewRef) {
                    jclass wvClass = env->GetObjectClass(localWebViewRef);
                    jmethodID sendPluginDrawMsg =
                            env->GetMethodID(wvClass, "sendPluginDrawMsg", "()V");
                    env->CallVoidMethod(localWebViewRef, sendPluginDrawMsg);
                    env->DeleteLocalRef(wvClass);
                    env->DeleteLocalRef(localWebViewRef);
                }
                checkException(env);
            }
        }

        // finally create a video texture if needed
        if (!m_newWindowRequest)
            return;

        // add the texture and add it to the list
        TextureWrapper* videoTexture = createTexture();
        m_videoTextures.append(videoTexture);

        // setup the state variables to signal the other thread
        m_newWindowRequest = false;
        m_newWindow = videoTexture->nativeWindow;
    }

    // signal the WebKit thread in case it is waiting
    m_newMediaRequestCond.signal();
}

void MediaTexture::draw(const TransformationMatrix& contentMatrix,
          const TransformationMatrix& videoMatrix,
          const SkRect& mediaBounds)
{
    android::Mutex::Autolock lock(m_mediaLock);

    if (mediaBounds.isEmpty())
        return;

    // draw all the video textures first
    for (unsigned int i = 0; i < m_videoTextures.size(); i++) {

        TextureWrapper* video = m_videoTextures[i];

        if (!video->surfaceTexture.get() || video->dimensions.isEmpty()
                || !video->mediaListener->isFrameAvailable())
            continue;

        video->surfaceTexture->updateTexImage();

        float surfaceMatrix[16];
        video->surfaceTexture->getTransformMatrix(surfaceMatrix);

        SkRect dimensions = video->dimensions;
        dimensions.offset(mediaBounds.fLeft, mediaBounds.fTop);

#ifdef DEBUG
        if (!mediaBounds.contains(dimensions)) {
            XLOG("The video exceeds is parent's bounds.");
        }
#endif // DEBUG

        TilesManager::instance()->shader()->drawVideoLayerQuad(videoMatrix,
                surfaceMatrix, dimensions, video->textureId);
    }

    if (!m_contentTexture->mediaListener->isFrameAvailable())
        return;

    m_contentTexture->surfaceTexture->updateTexImage();

    sp<GraphicBuffer> buf = m_contentTexture->surfaceTexture->getCurrentBuffer();

    PixelFormat f = buf->getPixelFormat();
    // only attempt to use alpha blending if alpha channel exists
    bool forceAlphaBlending = !(
        PIXEL_FORMAT_RGBX_8888 == f ||
        PIXEL_FORMAT_RGB_888 == f ||
        PIXEL_FORMAT_RGB_565 == f ||
        PIXEL_FORMAT_RGB_332 == f);

    TilesManager::instance()->shader()->drawLayerQuad(contentMatrix,
                                                      mediaBounds,
                                                      m_contentTexture->textureId,
                                                      1.0f, forceAlphaBlending,
                                                      GL_TEXTURE_EXTERNAL_OES);
}

ANativeWindow* MediaTexture::requestNativeWindowForVideo()
{
    android::Mutex::Autolock lock(m_mediaLock);

    // the window was not ready before the timeout so return it this time
    if (ANativeWindow* window = m_newWindow.get()) {
        m_newWindow.clear();
        return window;
    }

    // we only allow for so many textures, so return NULL if we exceed that limit
    else if (m_videoTextures.size() >= MAX_WINDOW_COUNT) {
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

    // if the window is ready then return it otherwise return NULL
    if (ANativeWindow* window = m_newWindow.get()) {
        m_newWindow.clear();
        return window;
    }
    return 0;
}

ANativeWindow* MediaTexture::getNativeWindowForContent()
{
    android::Mutex::Autolock lock(m_mediaLock);
    if (m_contentTexture)
        return m_contentTexture->nativeWindow.get();
    else
        return 0;
}

void MediaTexture::releaseNativeWindow(const ANativeWindow* window)
{
    android::Mutex::Autolock lock(m_mediaLock);
    for (unsigned int i = 0; i < m_videoTextures.size(); i++) {
        if (m_videoTextures[i]->nativeWindow.get() == window) {
            deleteTexture(m_videoTextures[i]);
            m_videoTextures.remove(i);
            break;
        }
    }
}

void MediaTexture::setDimensions(const ANativeWindow* window,
                                 const SkRect& dimensions)
{
    android::Mutex::Autolock lock(m_mediaLock);
    for (unsigned int i = 0; i < m_videoTextures.size(); i++) {
        if (m_videoTextures[i]->nativeWindow.get() == window) {
            m_videoTextures[i]->dimensions = dimensions;
            break;
        }
    }
}

void MediaTexture::setFramerateCallback(const ANativeWindow* window,
                                        FramerateCallbackProc callback)
{
    XLOG("Release ANW %p (%p):(%p)", this, m_surfaceTexture.get(), m_surfaceTextureClient.get());
    android::Mutex::Autolock lock(m_mediaLock);
    for (unsigned int i = 0; i < m_videoTextures.size(); i++) {
        if (m_videoTextures[i]->nativeWindow.get() == window) {
            m_videoTextures[i]->mediaListener->setFramerateCallback(callback);
            break;
        }
    }
}

MediaTexture::TextureWrapper* MediaTexture::createTexture()
{
    TextureWrapper* wrapper = new TextureWrapper();

    // populate the wrapper
    glGenTextures(1, &wrapper->textureId);
    wrapper->surfaceTexture = new android::SurfaceTexture(wrapper->textureId);
    wrapper->nativeWindow = new android::SurfaceTextureClient(wrapper->surfaceTexture);
    wrapper->dimensions.setEmpty();

    // setup callback
    wrapper->mediaListener = new MediaListener(m_weakWebViewRef,
                                               wrapper->surfaceTexture,
                                               wrapper->nativeWindow);
    wrapper->surfaceTexture->setFrameAvailableListener(wrapper->mediaListener);

    return wrapper;
}

void MediaTexture::deleteTexture(TextureWrapper* texture, bool force)
{
    if (texture->surfaceTexture.get())
        texture->surfaceTexture->setFrameAvailableListener(0);

    if (force)
        glDeleteTextures(1, &texture->textureId);
    else
        m_unusedTextures.append(texture->textureId);

    // clear the strong pointer references
    texture->mediaListener.clear();
    texture->nativeWindow.clear();
    texture->surfaceTexture.clear();

    delete texture;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
