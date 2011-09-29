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

#ifndef PaintedSurface_h
#define PaintedSurface_h

#include "BaseTileTexture.h"
#include "ClassTracker.h"
#include "IntRect.h"
#include "LayerAndroid.h"
#include "SkRefCnt.h"
#include "TextureOwner.h"
#include "TiledTexture.h"
#include "TilesManager.h"
#include "TilePainter.h"
#include "TransformationMatrix.h"

class SkCanvas;
class SkRegion;

namespace WebCore {

class PaintedSurface : public SkRefCnt, TilePainter {
public:
    PaintedSurface(LayerAndroid* layer)
        : m_layer(layer)
        , m_tiledTexture(0)
        , m_scale(0)
        , m_pictureUsed(0)
        , m_busy(false)
    {
        TilesManager::instance()->addPaintedSurface(this);
        SkSafeRef(m_layer);
#ifdef DEBUG_COUNT
        ClassTracker::instance()->increment("PaintedSurface");
#endif
        m_tiledTexture = new TiledTexture(this);
    }
    virtual ~PaintedSurface();

    // PaintedSurface methods

    LayerAndroid* layer() { return m_layer; }
    void prepare(GLWebViewState*);
    bool draw();
    void markAsDirty(const SkRegion& dirtyArea);
    bool paint(SkCanvas*);
    void removeLayer();
    void removeLayer(LayerAndroid* layer);
    void replaceLayer(LayerAndroid* layer);
    bool busy();

    bool owns(BaseTileTexture* texture);

    void computeVisibleArea();

    // TilePainter methods
    virtual bool paint(BaseTile*, SkCanvas*, unsigned int*);
    virtual void paintExtra(SkCanvas*);
    virtual const TransformationMatrix* transform();
    virtual void beginPaint();
    virtual void endPaint();

    // used by TiledTexture
    const IntRect& area() { return m_area; }
    const IntRect& visibleArea() { return m_visibleArea; }
    float scale() { return m_scale; }
    float opacity();
    unsigned int pictureUsed() { return m_pictureUsed; }
    TiledTexture* texture() { return m_tiledTexture; }

private:
    LayerAndroid* m_layer;
    TiledTexture* m_tiledTexture;

    IntRect m_area;
    IntRect m_visibleArea;
    float m_scale;

    unsigned int m_pictureUsed;

    bool m_busy;

    android::Mutex m_layerLock;
};

} // namespace WebCore

#endif // PaintedSurface_h
