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
#include "TiledTexture.h"

#include "TilesManager.h"
#include "TilesTracker.h"

#include "PaintedSurface.h"
#include "PaintTileOperation.h"
#include "SkCanvas.h"

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "TiledTexture", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "TiledTexture", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

void TiledTexture::prepare(GLWebViewState* state, bool repaint, bool startFastSwap)
{
    if (!m_surface)
        return;

    if (!m_surface->layer())
        return;

    // first, how many tiles do we need
    IntRect visibleArea = m_surface->visibleArea();
    IntRect area(visibleArea.x() * m_surface->scale(),
                 visibleArea.y() * m_surface->scale(),
                 ceilf(visibleArea.width() * m_surface->scale()),
                 ceilf(visibleArea.height() * m_surface->scale()));

    if (area.width() == 0 && area.height() == 0) {
        m_area.setWidth(0);
        m_area.setHeight(0);
        return;
    }

    int tileWidth = TilesManager::instance()->layerTileWidth();
    int tileHeight = TilesManager::instance()->layerTileHeight();

    m_area.setX(area.x() / tileWidth);
    m_area.setY(area.y() / tileHeight);
    float right = (area.x() + area.width()) / (float) tileWidth;
    float bottom = (area.y() + area.height()) / (float) tileHeight;
    m_area.setWidth(ceilf(right) - m_area.x());
    m_area.setHeight(ceilf(bottom) - m_area.y());

    XLOG("for TiledTexture %p, we have a visible area of %d, %d - %d x %d, corresponding to %d, %d x - %d x %d tiles",
         this,
         visibleArea.x(), visibleArea.y(),
         visibleArea.width(), visibleArea.height(),
         m_area.x(), m_area.y(),
         m_area.width(), m_area.height());

    bool goingDown = m_prevTileY < m_area.y();
    m_prevTileY = m_area.y();

    if (m_surface->scale() != m_prevScale)
        TilesManager::instance()->removeOperationsForFilter(new ScaleFilter(m_surface->scale()));

    m_prevScale = m_surface->scale();

    // unlock if tiles all ready
    bool tilesAllReady = true;
    for (unsigned int i = 0; i < m_tiles.size(); i++) {
        BaseTile* tile = m_tiles[i];
        if (tile->isTileVisible(m_area) && !tile->isTileReady()) {
            tilesAllReady = false;
            break;
        }
    }

    // startFastSwap=true will swap all ready tiles each
    // frame until all visible tiles are up to date
    if (tilesAllReady)
        m_swapWhateverIsReady = false;
    else if (startFastSwap)
        m_swapWhateverIsReady = true;

    // swap as appropriate
    for (unsigned int i = 0; i < m_tiles.size(); i++) {
        BaseTile* tile = m_tiles[i];
        if (tilesAllReady || m_swapWhateverIsReady)
            tile->swapTexturesIfNeeded();
    }

    if (tilesAllReady) {
        m_updateManager.swap();
        m_dirtyRegion.op(m_updateManager.getPaintingInval(), SkRegion::kUnion_Op);
        XLOG("TT %p swapping, now painting with picture %p"
             this, m_updateManager.getPaintingPicture());
        m_updateManager.clearPaintingInval();
    }

    // apply dirty region to affected tiles
    if (!m_dirtyRegion.isEmpty()) {
        for (unsigned int i = 0; i < m_tiles.size(); i++) {
            // TODO: don't mark all tiles dirty
            m_tiles[i]->markAsDirty(1, m_dirtyRegion);
        }
    }
    m_dirtyRegion.setEmpty();

    for (int i = 0; i < m_area.width(); i++) {
        if (goingDown) {
            for (int j = 0; j < m_area.height(); j++) {
                prepareTile(repaint, m_area.x() + i, m_area.y() + j);
            }
        } else {
            for (int j = m_area.height() - 1; j >= 0; j--) {
                prepareTile(repaint, m_area.x() + i, m_area.y() + j);
            }
        }
    }
}

void TiledTexture::update(const SkRegion& invalRegion, SkPicture* picture)
{
    XLOG("TT %p, update manager %p updated with picture %p, region empty %d",
          this, &m_updateManager, picture, invalRegion.isEmpty());
    // attempt to update inval and picture. these may be deferred below instead
    // of used immediately.
    m_updateManager.updateInval(invalRegion);
    m_updateManager.updatePicture(picture);
}

void TiledTexture::prepareTile(bool repaint, int x, int y)
{
    BaseTile* tile = getTile(x, y);
    if (!tile) {
        tile = new BaseTile(true);
        m_tiles.append(tile);
    }

    XLOG("preparing tile %p, painter is this %p", tile, this);
    tile->setContents(this, x, y, m_surface->scale());

    // TODO: move below (which is largely the same for layers / tiled page) into
    // prepare() function

    if (tile->isDirty() || !tile->frontTexture())
        tile->reserveTexture();
    LayerAndroid* layer = m_surface->layer();
    if (tile->backTexture() && tile->isDirty() && !tile->isRepaintPending() && layer) {
        PaintTileOperation *operation = new PaintTileOperation(tile, m_surface);
        TilesManager::instance()->scheduleOperation(operation);
    }
}

BaseTile* TiledTexture::getTile(int x, int y)
{
    for (unsigned int i = 0; i <m_tiles.size(); i++) {
        BaseTile* tile = m_tiles[i];
        if (tile->x() == x && tile->y() == y)
            return tile;
    }
    return 0;
}

bool TiledTexture::draw()
{
#ifdef DEBUG
    TilesManager::instance()->getTilesTracker()->trackLayer();
#endif

    if (m_area.width() == 0 || m_area.height() == 0)
        return false;

#ifdef DEBUG
    TilesManager::instance()->getTilesTracker()->trackVisibleLayer();
#endif

    float m_invScale = 1 / m_surface->scale();
    const float tileWidth = TilesManager::layerTileWidth() * m_invScale;
    const float tileHeight = TilesManager::layerTileHeight() * m_invScale;
    XLOG("draw tile %x, tiles %d", this, m_tiles.size());

    bool askRedraw = false;
    for (unsigned int i = 0; i < m_tiles.size(); i++) {
        BaseTile* tile = m_tiles[i];

        askRedraw |= !tile->isTileReady();
        if (tile->isTileVisible(m_area)) {
            SkRect rect;
            rect.fLeft = tile->x() * tileWidth;
            rect.fTop = tile->y() * tileHeight;
            rect.fRight = rect.fLeft + tileWidth;
            rect.fBottom = rect.fTop + tileHeight;
            XLOG(" - [%d], { painter %x vs %x }, tile %x %d,%d at scale %.2f [ready: %d] dirty: %d",
                 i, this, tile->painter(), tile, tile->x(), tile->y(),
                 tile->scale(), tile->isTileReady(), tile->isDirty());
            tile->draw(m_surface->opacity(), rect, m_surface->scale());
#ifdef DEBUG
            TilesManager::instance()->getTilesTracker()->track(tile->isTileReady(), tile->backTexture());
#endif
        }
    }

    // need to redraw if some visible tile wasn't ready
    return askRedraw;
}

bool TiledTexture::paint(BaseTile* tile, SkCanvas* canvas, unsigned int* pictureUsed)
{
    return m_updateManager.paint(tile, canvas, pictureUsed);
}

void TiledTexture::paintExtra(SkCanvas* canvas)
{
    m_surface->paintExtra(canvas);
}

const TransformationMatrix* TiledTexture::transform()
{
    return m_surface->transform();
}

void TiledTexture::removeTiles()
{
    for (unsigned int i = 0; i < m_tiles.size(); i++) {
        delete m_tiles[i];
    }
}

bool TiledTexture::owns(BaseTileTexture* texture)
{
    for (unsigned int i = 0; i < m_tiles.size(); i++) {
        BaseTile* tile = m_tiles[i];
        if (tile->frontTexture() == texture)
            return true;
        if (tile->backTexture() == texture)
            return true;
    }
    return false;
}

} // namespace WebCore
