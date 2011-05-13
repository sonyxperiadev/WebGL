/*
 * Copyright (C) 2010 The Android Open Source Project
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
#include "MediaLayer.h"
#include "MediaTexture.h"
#include "TilesManager.h"

#if USE(ACCELERATED_COMPOSITING)

#define LAYER_DEBUG
#undef LAYER_DEBUG

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "MediaLayer", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

MediaLayer::MediaLayer(jobject weakWebViewRef) : LayerAndroid((RenderLayer*) NULL)
{
    m_bufferedTexture = new MediaTexture(EGL_NO_CONTEXT);
    m_bufferedTexture->producerInc();
    m_videoTexture = new VideoTexture(weakWebViewRef);
    m_videoTexture->incStrong(this);

    m_isCopy = false;
    m_currentTextureInfo = 0;
    m_isContentInverted = false;
    m_outlineSize = 0;
    XLOG("Creating Media Layer %p", this);
    XLOG("producer: %d consumer: %d", m_bufferedTexture->getProducerCount(),
            m_bufferedTexture->getConsumerCount());
}

MediaLayer::MediaLayer(const MediaLayer& layer) : LayerAndroid(layer)
{
    m_bufferedTexture = layer.getTexture();
    m_bufferedTexture->consumerInc();
    m_videoTexture = layer.m_videoTexture;
    m_videoTexture->incStrong(this);

    m_isCopy = true;
    m_currentTextureInfo = 0;
    m_isContentInverted = layer.m_isContentInverted;
    m_outlineSize = layer.m_outlineSize;
    XLOG("Creating Media Layer Copy %p -> %p", &layer, this);
    XLOG("producer: %d consumer: %d COPY", m_bufferedTexture->getProducerCount(),
            m_bufferedTexture->getConsumerCount());
}

MediaLayer::~MediaLayer()
{
    XLOG("Deleting Media Layer");
    XLOG("producer: %d consumer: %d %s", m_bufferedTexture->getProducerCount(),
            m_bufferedTexture->getConsumerCount(), (m_isCopy) ? "COPY" : "");

    if (m_isCopy)
        m_bufferedTexture->consumerDec();
    else
        m_bufferedTexture->producerDec();
    m_videoTexture->decStrong(this);
}

bool MediaLayer::drawGL(GLWebViewState* glWebViewState, SkMatrix& matrix)
{
    TilesManager::instance()->shader()->clip(drawClip());

    // when the plugin gains focus webkit applies an outline to the
    // widget, which causes the layer to expand to accommodate the
    // outline. Therefore, we shrink the rect by the outline's dimensions
    // to ensure the plugin does not draw outside of its bounds.
    SkRect mediaBounds;
    mediaBounds.set(0, 0, getSize().width(), getSize().height());
    mediaBounds.inset(m_outlineSize, m_outlineSize);

    // check to see if we need to create a video texture
    m_videoTexture->initNativeWindowIfNeeded();
    // draw any video content if present
    m_videoTexture->drawVideo(drawTransform(), mediaBounds);

    // draw the primary content
    if (m_bufferedTexture) {
        TextureInfo* textureInfo = m_bufferedTexture->consumerLock();
        if (textureInfo && textureInfo->m_width != 0 && textureInfo->m_height != 0) {
            // the layer's shader draws the content inverted so we must undo
            // that change in the transformation matrix
            TransformationMatrix m = drawTransform();
            if (!m_isContentInverted) {
                m.flipY();
                m.translate(0, -getSize().height());
            }

            bool forceBlending = textureInfo->m_internalFormat == GL_RGBA ||
                                 textureInfo->m_internalFormat == GL_BGRA_EXT ||
                                 textureInfo->m_internalFormat == GL_ALPHA;
            TilesManager::instance()->shader()->drawLayerQuad(m, mediaBounds,
                                                              textureInfo->m_textureId,
                                                              1.0f, forceBlending);
        }
        m_bufferedTexture->consumerRelease();
    }

    return drawChildrenGL(glWebViewState, matrix);
}

ANativeWindow* MediaLayer::acquireNativeWindowForVideo()
{
    return m_videoTexture->requestNewWindow();
}

void MediaLayer::setWindowDimensionsForVideo(const ANativeWindow* window, const SkRect& dimensions)
{
    if (window != m_videoTexture->getNativeWindow())
        return;

    //TODO validate that the dimensions do not exceed the plugin's bounds
    m_videoTexture->setDimensions(dimensions);
}

void MediaLayer::releaseNativeWindowForVideo(ANativeWindow* window)
{
    if (window == m_videoTexture->getNativeWindow())
        m_videoTexture->releaseNativeWindow();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
