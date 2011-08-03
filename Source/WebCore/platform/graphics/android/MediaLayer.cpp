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
    m_contentTexture = new MediaTexture(weakWebViewRef);
    m_contentTexture->incStrong(this);
    m_videoTexture = new MediaTexture(weakWebViewRef);
    m_videoTexture->incStrong(this);

    m_isCopy = false;
    m_isContentInverted = false;
    m_outlineSize = 0;
    XLOG("Creating Media Layer %p", this);
}

MediaLayer::MediaLayer(const MediaLayer& layer) : LayerAndroid(layer)
{
    m_contentTexture = layer.m_contentTexture;
    m_contentTexture->incStrong(this);
    m_videoTexture = layer.m_videoTexture;
    m_videoTexture->incStrong(this);

    m_isCopy = true;
    m_isContentInverted = layer.m_isContentInverted;
    m_outlineSize = layer.m_outlineSize;
    XLOG("Creating Media Layer Copy %p -> %p", &layer, this);
}

MediaLayer::~MediaLayer()
{
    XLOG("Deleting Media Layer");
    m_contentTexture->decStrong(this);
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
    m_videoTexture->drawVideo(m_drawTransform, mediaBounds);

    // the layer's shader draws the content inverted so we must undo
    // that change in the transformation matrix
    TransformationMatrix m = m_drawTransform;
    if (!m_isContentInverted) {
        m.flipY();
        m.translate(0, -getSize().height());
    }

    // check to see if we need to create a content texture
    m_contentTexture->initNativeWindowIfNeeded();
    // draw any content if present
    m_contentTexture->setDimensions(mediaBounds);
    m_contentTexture->drawContent(m);

    return drawChildrenGL(glWebViewState, matrix);
}

ANativeWindow* MediaLayer::acquireNativeWindowForContent()
{
    return m_contentTexture->requestNewWindow();
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
