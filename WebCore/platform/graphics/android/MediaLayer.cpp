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
#include "TilesManager.h"

#if USE(ACCELERATED_COMPOSITING)

#include <wtf/CurrentTime.h>

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

MediaLayer::MediaLayer() : LayerAndroid(false)
{
    m_bufferedTexture = new MediaTexture(EGL_NO_CONTEXT);
    m_bufferedTexture->incStrong(this);
    m_currentTextureInfo = 0;
    m_isContentInverted = false;
    XLOG("Creating Media Layer %p", this);
}

MediaLayer::MediaLayer(const MediaLayer& layer) : LayerAndroid(layer)
{
    m_bufferedTexture = layer.getTexture();
    m_bufferedTexture->incStrong(this);
    m_currentTextureInfo = 0;
    m_isContentInverted = layer.m_isContentInverted;
    XLOG("Creating Media Layer Copy %p -> %p", &layer, this);
}

MediaLayer::~MediaLayer()
{
    XLOG("Deleting Media Layer");
    m_bufferedTexture->decStrong(this);
}

bool MediaLayer::drawGL(SkMatrix& matrix)
{
    if (m_bufferedTexture) {
        TextureInfo* textureInfo = m_bufferedTexture->consumerLock();
        if (textureInfo) {
            SkRect rect;
            rect.set(0, 0, getSize().width(), getSize().height());
            TransformationMatrix m = drawTransform();

            // the layer's shader draws the content inverted so we must undo
            // that change in the transformation matrix
            if (!m_isContentInverted) {
                m.flipY();
                m.translate(0, -getSize().height());
            }

            TilesManager::instance()->shader()->drawLayerQuad(m, rect,
                                                              textureInfo->m_textureId,
                                                              1.0f); //TODO fix this m_drawOpacity
        }
        m_bufferedTexture->consumerRelease();
    }

    drawChildrenGL(matrix);

    //TODO allow plugins to specify when they should be drawn
    return true;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
