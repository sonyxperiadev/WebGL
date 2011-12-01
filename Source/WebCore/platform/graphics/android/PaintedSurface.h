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
#include "TilesManager.h"
#include "TilePainter.h"
#include "TransformationMatrix.h"

class SkCanvas;
class SkRegion;

namespace WebCore {

class DualTiledTexture;

class PaintedSurface : public SurfacePainter {
public:
    PaintedSurface();
    virtual ~PaintedSurface();

    // PaintedSurface methods

    void prepare(GLWebViewState*);
    bool draw();
    bool paint(SkCanvas*);

    void setDrawingLayer(LayerAndroid* layer) { m_drawingLayer = layer; }
    LayerAndroid* drawingLayer() { return m_drawingLayer; }

    void setPaintingLayer(LayerAndroid* layer, const SkRegion& dirtyArea);
    void clearPaintingLayer() { m_paintingLayer = 0; }
    LayerAndroid* paintingLayer() { return m_paintingLayer; }

    void swapTiles();
    bool isReady();

    bool owns(BaseTileTexture* texture);

    void computeTexturesAmount(TexturesResult*);
    IntRect computeVisibleArea(LayerAndroid*);

    // TilePainter methods for TiledTexture
    virtual const TransformationMatrix* transform();
    virtual float opacity();

    // used by TiledTexture
    float scale() { return m_scale; }
    unsigned int pictureUsed() { return m_pictureUsed; }

private:
    LayerAndroid* m_drawingLayer;
    LayerAndroid* m_paintingLayer;
    DualTiledTexture* m_tiledTexture;

    float m_scale;

    unsigned int m_pictureUsed;

    android::Mutex m_layerLock;
};

} // namespace WebCore

#endif // PaintedSurface_h
