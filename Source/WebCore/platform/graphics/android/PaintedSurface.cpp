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
#include "TiledTexture.h"
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

// Layers with an area larger than 2048*2048 should never be unclipped
#define MAX_UNCLIPPED_AREA 4194304

namespace WebCore {

PaintedSurface::PaintedSurface()
    : m_drawingLayer(0)
    , m_paintingLayer(0)
    , m_tiledTexture(0)
    , m_scale(0)
    , m_pictureUsed(0)
{
    TilesManager::instance()->addPaintedSurface(this);
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("PaintedSurface");
#endif
    m_tiledTexture = new DualTiledTexture(this);
}

PaintedSurface::~PaintedSurface()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("PaintedSurface");
#endif
    delete m_tiledTexture;
}

void PaintedSurface::prepare(GLWebViewState* state)
{
    XLOG("PS %p has PL %p, DL %p", this, m_paintingLayer, m_drawingLayer);
    LayerAndroid* paintingLayer = m_paintingLayer;
    if (!paintingLayer)
        paintingLayer = m_drawingLayer;

    if (!paintingLayer)
        return;

    bool startFastSwap = false;
    if (state->isScrolling()) {
        // when scrolling, block updates and swap tiles as soon as they're ready
        startFastSwap = true;
    }

    XLOG("prepare layer %d %x at scale %.2f",
         paintingLayer->uniqueId(), paintingLayer,
         paintingLayer->getScale());

    IntRect visibleArea = computeVisibleArea(paintingLayer);

    m_scale = state->scale();

    // If we do not have text, we may as well limit ourselves to
    // a scale factor of one... this saves up textures.
    if (m_scale > 1 && !paintingLayer->hasText())
        m_scale = 1;

    m_tiledTexture->prepare(state, m_scale, m_pictureUsed != paintingLayer->pictureUsed(),
                            startFastSwap, visibleArea);
}

bool PaintedSurface::draw()
{
    if (!m_drawingLayer || !m_drawingLayer->needsTexture())
        return false;

    bool askRedraw = false;
    if (m_tiledTexture)
        askRedraw = m_tiledTexture->draw();

    return askRedraw;
}

void PaintedSurface::setPaintingLayer(LayerAndroid* layer, const SkRegion& dirtyArea)
{
    m_paintingLayer = layer;
    if (m_tiledTexture)
        m_tiledTexture->update(dirtyArea, layer->picture());
}

bool PaintedSurface::isReady()
{
    if (m_tiledTexture)
        return m_tiledTexture->isReady();
    return false;
}

void PaintedSurface::swapTiles()
{
    if (m_tiledTexture)
        m_tiledTexture->swapTiles();
}

float PaintedSurface::opacity() {
    if (m_drawingLayer)
        return m_drawingLayer->drawOpacity();
    return 1.0;
}

const TransformationMatrix* PaintedSurface::transform() {
    // used exclusively for drawing, so only use m_drawingLayer
    if (!m_drawingLayer)
        return 0;

    return m_drawingLayer->drawTransform();
}

void PaintedSurface::computeTexturesAmount(TexturesResult* result)
{
    if (!m_tiledTexture)
        return;

    // for now, always done on drawinglayer
    LayerAndroid* layer = m_drawingLayer;

    if (!layer)
        return;

    IntRect unclippedArea = layer->unclippedArea();
    IntRect clippedVisibleArea = layer->visibleArea();
    // get two numbers here:
    // - textures needed for a clipped area
    // - textures needed for an un-clipped area
    int nbTexturesUnclipped = m_tiledTexture->nbTextures(unclippedArea, m_scale);
    int nbTexturesClipped = m_tiledTexture->nbTextures(clippedVisibleArea, m_scale);

    // Set kFixedLayers level
    if (layer->isFixed())
        result->fixed += nbTexturesClipped;

    // Set kScrollableAndFixedLayers level
    if (layer->contentIsScrollable()
        || layer->isFixed())
        result->scrollable += nbTexturesClipped;

    // Set kClippedTextures level
    result->clipped += nbTexturesClipped;

    // Set kAllTextures level
    if (layer->contentIsScrollable())
        result->full += nbTexturesClipped;
    else
        result->full += nbTexturesUnclipped;
}

IntRect PaintedSurface::computeVisibleArea(LayerAndroid* layer) {
    IntRect area;
    if (!layer)
        return area;

    if (!layer->contentIsScrollable()
        && layer->state()->layersRenderingMode() == GLWebViewState::kAllTextures) {
        area = layer->unclippedArea();
        double total = ((double) area.width()) * ((double) area.height());
        if (total > MAX_UNCLIPPED_AREA)
            area = layer->visibleArea();
    } else {
        area = layer->visibleArea();
    }

    return area;
}

bool PaintedSurface::owns(BaseTileTexture* texture)
{
    if (m_tiledTexture)
        return m_tiledTexture->owns(texture);
    return false;
}

} // namespace WebCore
