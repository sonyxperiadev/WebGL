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

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "PaintedSurface", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "PaintedSurface", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

// Allows layers using less than MAX_UNCLIPPED_AREA tiles to
// schedule all of them instead of clipping the area with the visible rect.
#define MAX_UNCLIPPED_AREA 16

namespace WebCore {

PaintedSurface::PaintedSurface(LayerAndroid* layer)
    : m_layer(layer)
    , m_tiledTexture(0)
    , m_scale(0)
    , m_pictureUsed(0)
{
    TilesManager::instance()->addPaintedSurface(this);
    SkSafeRef(m_layer);
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("PaintedSurface");
#endif
    m_tiledTexture = new DualTiledTexture(this);
    if (layer && layer->picture())
        m_updateManager.updatePicture(layer->picture());
}

PaintedSurface::~PaintedSurface()
{
    XLOG("dtor of %x m_layer: %x", this, m_layer);
    android::Mutex::Autolock lock(m_layerLock);
    SkSafeUnref(m_layer);
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("PaintedSurface");
#endif
    delete m_tiledTexture;
}

void PaintedSurface::removeLayer()
{
    android::Mutex::Autolock lock(m_layerLock);
    if (m_layer)
        m_layer->removeTexture(this);
    SkSafeUnref(m_layer);
    m_layer = 0;
}

void PaintedSurface::removeLayer(LayerAndroid* layer)
{
    android::Mutex::Autolock lock(m_layerLock);
    if (m_layer != layer)
        return;
    SkSafeUnref(m_layer);
    m_layer = 0;
}

void PaintedSurface::replaceLayer(LayerAndroid* layer)
{
    android::Mutex::Autolock lock(m_layerLock);
    if (!layer)
        return;

    if (m_layer && layer->uniqueId() != m_layer->uniqueId())
        return;

    SkSafeRef(layer);
    SkSafeUnref(m_layer);
    m_layer = layer;
    if (layer && layer->picture())
        m_updateManager.updatePicture(layer->picture());
}

void PaintedSurface::prepare(GLWebViewState* state)
{
    if (!m_layer)
        return;

    if (!m_layer->needsTexture())
        return;

    bool startFastSwap = false;
    if (state->isScrolling()) {
        // when scrolling, block updates and swap tiles as soon as they're ready
        startFastSwap = true;
    } else {
        // when not, push updates down to TiledTexture in every prepare
        m_updateManager.swap();
        m_tiledTexture->update(m_updateManager.getPaintingInval(),
                               m_updateManager.getPaintingPicture());
        m_updateManager.clearPaintingInval();
    }

    XLOG("prepare layer %d %x at scale %.2f",
         m_layer->uniqueId(), m_layer,
         m_layer->getScale());

    int w = m_layer->getSize().width();
    int h = m_layer->getSize().height();

    if (w != m_area.width())
        m_area.setWidth(w);

    if (h != m_area.height())
        m_area.setHeight(h);

    computeVisibleArea();

    m_scale = state->scale();

    XLOG("%x layer %d %x prepared at size (%d, %d) @ scale %.2f", this, m_layer->uniqueId(),
         m_layer, w, h, m_scale);

    m_tiledTexture->prepare(state, m_scale, m_pictureUsed != m_layer->pictureUsed(),
                            startFastSwap, m_visibleArea);
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

void PaintedSurface::markAsDirty(const SkRegion& dirtyArea)
{
    m_updateManager.updateInval(dirtyArea);
}

void PaintedSurface::paintExtra(SkCanvas* canvas)
{
    m_layerLock.lock();
    LayerAndroid* layer = m_layer;
    SkSafeRef(layer);
    m_layerLock.unlock();

    if (layer)
        layer->extraDraw(canvas);

    SkSafeUnref(layer);
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
    if (!m_visibleArea.isEmpty()) {
        float tileWidth = TilesManager::instance()->layerTileWidth();
        float tileHeight = TilesManager::instance()->layerTileHeight();
        int w = ceilf(m_area.width() * m_scale / tileWidth);
        int h = ceilf(m_area.height() * m_scale / tileHeight);
        if (w * h < MAX_UNCLIPPED_AREA)
            m_visibleArea = m_area;
    }
}

bool PaintedSurface::owns(BaseTileTexture* texture)
{
    if (m_tiledTexture)
        return m_tiledTexture->owns(texture);
    return false;
}

} // namespace WebCore
