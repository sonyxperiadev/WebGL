/*
 * Copyright 2010, The Android Open Source Project
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

#ifndef PaintTileSetOperation_h
#define PaintTileSetOperation_h

#include "BaseTile.h"
#include "QueuedOperation.h"
#include "SkRefCnt.h"

namespace WebCore {

class LayerAndroid;
class SurfacePainter;
class ImageTexture;

class PaintTileOperation : public QueuedOperation {
public:
    PaintTileOperation(BaseTile* tile, SurfacePainter* surface = 0);
    virtual ~PaintTileOperation();
    virtual bool operator==(const QueuedOperation* operation);
    virtual void run();
    // returns a rendering priority for m_tile, lower values are processed faster
    virtual int priority();
    TilePainter* painter() { return m_tile->painter(); }
    float scale() { return m_tile->scale(); }

private:
    BaseTile* m_tile;
    SurfacePainter* m_surface;
};

class ScaleFilter : public OperationFilter {
public:
    ScaleFilter(TilePainter* painter, float scale)
        : m_painter(painter)
        , m_scale(scale) {}
    virtual bool check(QueuedOperation* operation)
    {
        if (operation->type() == QueuedOperation::PaintTile) {
            PaintTileOperation* op = static_cast<PaintTileOperation*>(operation);
            if ((op->painter() == m_painter) && (op->scale() != m_scale))
                return true;
        }
        return false;
    }
private:
    TilePainter* m_painter;
    float m_scale;
};


class TilePainterFilter : public OperationFilter {
public:
    TilePainterFilter(TilePainter* painter) : m_painter(painter) {}
    virtual bool check(QueuedOperation* operation)
    {
        if (operation->type() == QueuedOperation::PaintTile) {
            PaintTileOperation* op = static_cast<PaintTileOperation*>(operation);
            if (op->painter() == m_painter)
                return true;
        }
        return false;
    }
private:
    TilePainter* m_painter;
};

}

#endif // PaintTileSetOperation_h
