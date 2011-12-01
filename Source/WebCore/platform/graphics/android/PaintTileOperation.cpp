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
#include "ImageTexture.h"
#include "ImagesManager.h"
#include "LayerAndroid.h"
#include "PaintedSurface.h"

namespace WebCore {

PaintTileOperation::PaintTileOperation(BaseTile* tile, SurfacePainter* surface)
    : QueuedOperation(QueuedOperation::PaintTile, tile->page())
    , m_tile(tile)
    , m_surface(surface)
{
    if (m_tile)
        m_tile->setRepaintPending(true);
    SkSafeRef(m_surface);
}

PaintTileOperation::~PaintTileOperation()
{
    if (m_tile) {
        m_tile->setRepaintPending(false);
        m_tile = 0;
    }

    if (m_surface && m_surface->type() == SurfacePainter::ImageSurface) {
        ImageTexture* image = static_cast<ImageTexture*>(m_surface);
        ImagesManager::instance()->releaseImage(image->imageCRC());
    } else {
        SkSafeUnref(m_surface);
    }
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

    int priority = 200000;

    // if scrolling, prioritize the prefetch page, otherwise deprioritize
    TiledPage* page = m_tile->page();
    if (page && page->isPrefetchPage()) {
        if (page->glWebViewState()->isScrolling())
            priority = 0;
        else
            priority = 400000;
    }

    // prioritize higher draw count
    unsigned long long currentDraw = TilesManager::instance()->getDrawGLCount();
    unsigned long long drawDelta = currentDraw - m_tile->drawCount();
    priority += 100000 * (int)std::min(drawDelta, (unsigned long long)1000);

    // prioritize unpainted tiles, within the same drawCount
    if (m_tile->frontTexture())
        priority += 50000;

    // for base tiles, prioritize based on position
    if (!m_tile->isLayerTile()) {
        bool goingDown = m_tile->page()->scrollingDown();
        priority += m_tile->x();

        if (goingDown)
            priority += 100000 - (1 + m_tile->y()) * 1000;
        else
            priority += m_tile->y() * 1000;
    }

    return priority;
}

}
