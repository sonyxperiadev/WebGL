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

#ifndef TiledTexture_h
#define TiledTexture_h

#include "BaseTile.h"
#include "BaseTileTexture.h"
#include "ClassTracker.h"
#include "IntRect.h"
#include "LayerAndroid.h"
#include "SkRegion.h"
#include "TextureOwner.h"
#include "TilePainter.h"

class SkCanvas;

namespace WebCore {

class PaintedSurface;

class TiledTexture : public TilePainter {
public:
    TiledTexture(PaintedSurface* surface)
        : m_surface(surface)
        , m_prevTileX(0)
        , m_prevTileY(0)
        , m_prevScale(1)
    {
        m_dirtyRegion.setEmpty();
#ifdef DEBUG_COUNT
        ClassTracker::instance()->increment("TiledTexture");
#endif
    }
    virtual ~TiledTexture()
    {
#ifdef DEBUG_COUNT
        ClassTracker::instance()->decrement("TiledTexture");
#endif
        removeTiles();
    };

    void prepare(GLWebViewState* state, bool repaint);
    bool draw();

    void prepareTile(bool repaint, int x, int y);
    void markAsDirty(const SkRegion& dirtyArea);

    BaseTile* getTile(int x, int y);

    void removeTiles();
    bool owns(BaseTileTexture* texture);

    // TilePainter methods
    bool paint(BaseTile* tile, SkCanvas*, unsigned int*);
    virtual void paintExtra(SkCanvas*);
    virtual const TransformationMatrix* transform();
    virtual void beginPaint();
    virtual void endPaint();

private:
    PaintedSurface* m_surface;
    Vector<BaseTile*> m_tiles;

    IntRect m_area;
    SkRegion m_dirtyRegion;

    int m_prevTileX;
    int m_prevTileY;
    float m_prevScale;
};

} // namespace WebCore

#endif // TiledTexture_h
