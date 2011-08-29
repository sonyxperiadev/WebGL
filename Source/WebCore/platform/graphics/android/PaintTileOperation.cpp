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
#include "PaintTileOperation.h"
#include "LayerAndroid.h"
#include "PaintedSurface.h"

namespace WebCore {

PaintTileOperation::PaintTileOperation(BaseTile* tile, PaintedSurface* surface)
    : QueuedOperation(QueuedOperation::PaintTile, tile->page())
    , m_tile(tile)
    , m_surface(surface)
    , m_layer(0)
{
    if (m_tile)
        m_tile->setRepaintPending(true);
    if (m_surface)
        m_layer = surface->layer();
    SkSafeRef(m_surface);
    SkSafeRef(m_layer);
}

PaintTileOperation::~PaintTileOperation()
{
    if (m_tile) {
        m_tile->setRepaintPending(false);
        m_tile = 0;
    }
    SkSafeUnref(m_surface);
    SkSafeUnref(m_layer);
}

bool PaintTileOperation::operator==(const QueuedOperation* operation)
{
    if (operation->type() != type())
        return false;
    const PaintTileOperation* op = static_cast<const PaintTileOperation*>(operation);
    return op->m_tile == m_tile;
}

void PaintTileOperation::run()
{
    if (m_tile) {
        m_tile->paintBitmap();
        m_tile->setRepaintPending(false);
        m_tile = 0;
    }
}

int PaintTileOperation::priority()
{
    if (!m_tile)
        return -1;

    int priority;
    if (m_tile->isLayerTile())
        priority = -2;
    else {
        bool goingDown = m_tile->page()->scrollingDown();
        SkIRect *rect = m_tile->page()->expandedTileBounds();
        int firstTileX = rect->fLeft;
        int nbTilesWidth = rect->width();
        priority = m_tile->x() - firstTileX;
        if (goingDown)
            priority += (rect->fBottom - m_tile->y()) * nbTilesWidth;
        else
            priority += (m_tile->y() - rect->fTop) * nbTilesWidth;
    }

    if (m_tile->frontTexture()) {
        // de-prioritize old tiles that have something visible
        unsigned long long currentDraw = TilesManager::instance()->getDrawGLCount();
        unsigned long long drawDelta = currentDraw - m_tile->drawCount();
        int cappedDrawDelta = (int)std::max(drawDelta, (unsigned long long)1000);
        priority += cappedDrawDelta * 100000;
    }

    return priority;
}

}
