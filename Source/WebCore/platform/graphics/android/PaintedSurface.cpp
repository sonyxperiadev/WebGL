/*
 * Copyright 2011, The Android Open Source Project
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PaintedSurface.h"

#include "LayerAndroid.h"
#include "TilesManager.h"
#include "SkCanvas.h"
#include "SkPicture.h"

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "PaintedSurface", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

bool PaintedSurface::busy()
{
    android::Mutex::Autolock lock(m_layerLock);
    return m_busy;
}

void PaintedSurface::removeLayer(LayerAndroid* layer)
{
    android::Mutex::Autolock lock(m_layerLock);
    if (m_layer != layer)
        return;
    m_layer = 0;
}

void PaintedSurface::replaceLayer(LayerAndroid* layer)
{
    android::Mutex::Autolock lock(m_layerLock);
    if (!layer)
        return;

    if (m_layer && layer->uniqueId() != m_layer->uniqueId())
        return;

    m_layer = layer;
}

void PaintedSurface::prepare(GLWebViewState* state)
{
    if (!m_layer)
        return;

    if (!m_layer->needsTexture())
        return;

    XLOG("prepare layer %d %x at scale %.2f",
         m_layer->uniqueId(), m_layer,
         m_layer->getScale());

    float scale = m_layer->getScale();
    int w = m_layer->getSize().width();
    int h = m_layer->getSize().height();

    if (w != m_area.width())
        m_area.setWidth(w);

    if (h != m_area.height())
        m_area.setHeight(h);

    computeVisibleArea();

    if (scale != m_scale)
        m_scale = scale;

    XLOG("layer %d %x prepared at size (%d, %d) @ scale %.2f", m_layer->uniqueId(),
         m_layer, w, h, scale);

    if (!m_tiledTexture)
        m_tiledTexture = new TiledTexture(this);

    m_tiledTexture->prepare(state, m_pictureUsed != m_layer->pictureUsed());
}

bool PaintedSurface::draw()
{
    if (!m_layer || !m_layer->needsTexture())
        return false;

    bool askRedraw = false;
    if (m_tiledTexture)
        askRedraw = m_tiledTexture->draw();
    return askRedraw;
}

void PaintedSurface::beginPaint()
{
    m_layerLock.lock();
    m_busy = true;
    m_layerLock.unlock();
}

void PaintedSurface::endPaint()
{
    m_layerLock.lock();
    m_busy = false;
    m_layerLock.unlock();
}

bool PaintedSurface::paint(BaseTile* tile, SkCanvas* canvas, unsigned int* pictureUsed)
{
    m_layerLock.lock();
    LayerAndroid* layer = m_layer;
    m_layerLock.unlock();

    if (!layer)
        return false;

    layer->contentDraw(canvas);
    m_pictureUsed = layer->pictureUsed();
    *pictureUsed = m_pictureUsed;

    return true;
}

void PaintedSurface::paintExtra(SkCanvas* canvas)
{
    m_layerLock.lock();
    LayerAndroid* layer = m_layer;
    m_layerLock.unlock();

    if (layer)
        layer->extraDraw(canvas);
}

float PaintedSurface::opacity() {
    if (m_layer)
        return m_layer->drawOpacity();
    return 1.0;
}

const TransformationMatrix* PaintedSurface::transform() {
    if (!m_layer)
        return 0;

    return m_layer->drawTransform();
}

void PaintedSurface::computeVisibleArea() {
    if (!m_layer)
        return;
    IntRect layerRect = (*m_layer->drawTransform()).mapRect(m_area);
    IntRect clippedRect = TilesManager::instance()->shader()->clippedRectWithViewport(layerRect);
    m_visibleArea = (*m_layer->drawTransform()).inverse().mapRect(clippedRect);
}

bool PaintedSurface::owns(BaseTileTexture* texture)
{
    if (m_tiledTexture)
        return m_tiledTexture->owns(texture);
    return false;
}

} // namespace WebCore
