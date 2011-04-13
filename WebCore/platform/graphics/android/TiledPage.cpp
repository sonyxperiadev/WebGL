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

#include "config.h"
#include "TiledPage.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GLUtils.h"
#include "IntRect.h"
#include "PaintTileOperation.h"
#include "TilesManager.h"

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "TiledPage", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

using namespace android;

TiledPage::TiledPage(int id, GLWebViewState* state)
    : m_baseTiles(0)
    , m_baseTileSize(0)
    , m_id(id)
    , m_scale(1)
    , m_invScale(1)
    , m_glWebViewState(state)
    , m_latestPictureInval(0)
    , m_prepare(false)
{
    m_baseTiles = new BaseTile[TilesManager::getMaxTextureAllocation() + 1];
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("TiledPage");
#endif
}

void TiledPage::updateBaseTileSize()
{
    // This value must be at least 1 greater than the max number of allowed
    // textures. This is because prepare() asks for a tile before it reserves
    // a texture for that tile. If all textures are currently in use by the
    // page then there will be no available tile and having the extra tile
    // ensures that this does not happen. After claiming the extra tile the call
    // to reserveTexture() will cause some other tile in the page to lose it's
    // texture and become available, thus ensuring that we always have at least
    // one tile that is available.
    int baseTileSize = TilesManager::instance()->maxTextureCount() + 1;
    if (baseTileSize > m_baseTileSize)
        m_baseTileSize = baseTileSize;
}

TiledPage::~TiledPage()
{
    // In order to delete the page we must ensure that none of its BaseTiles are
    // currently painting or scheduled to be painted by the TextureGenerator
    TilesManager::instance()->removeOperationsForPage(this);
    delete[] m_baseTiles;
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("TiledPage");
#endif
}

BaseTile* TiledPage::getBaseTile(int x, int y) const
{
    for (int j = 0; j < m_baseTileSize; j++) {
        BaseTile& tile = m_baseTiles[j];
        if (tile.x() == x && tile.y() == y && !tile.isAvailable())
            return &tile;
    }
    return 0;
}

void TiledPage::setUsable(bool usable)
{
    for (int j = 0; j < m_baseTileSize; j++) {
        BaseTile& tile = m_baseTiles[j];
        tile.setUsable(usable);
    }
    return;
}

void TiledPage::invalidateRect(const IntRect& inval, const unsigned int pictureCount)
{
    // Given the current scale level we need to mark the appropriate tiles as dirty
    const float invTileContentWidth = m_scale / TilesManager::tileWidth();
    const float invTileContentHeight = m_scale / TilesManager::tileHeight();

    const int firstDirtyTileX = static_cast<int>(floorf(inval.x() * invTileContentWidth));
    const int firstDirtyTileY = static_cast<int>(floorf(inval.y() * invTileContentHeight));
    const int lastDirtyTileX = static_cast<int>(ceilf(inval.right() * invTileContentWidth));
    const int lastDirtyTileY = static_cast<int>(ceilf(inval.bottom() * invTileContentHeight));

    XLOG("Marking X %d-%d and Y %d-%d dirty", firstDirtyTileX, lastDirtyTileX, firstDirtyTileY, lastDirtyTileY);
    // We defer marking the tile as dirty until the next time we need to prepare
    // to draw.
    m_invalRegion.op(firstDirtyTileX, firstDirtyTileY, lastDirtyTileX, lastDirtyTileY, SkRegion::kUnion_Op);
    m_invalTilesRegion.op(inval.x(), inval.y(), inval.right(), inval.bottom(), SkRegion::kUnion_Op);
    m_latestPictureInval = pictureCount;
}

void TiledPage::prepareRow(bool goingLeft, int tilesInRow, int firstTileX, int y, const SkIRect& tileBounds)
{
    if (y < 0)
        return;

    for (int i = 0; i < tilesInRow; i++) {
        int x = firstTileX;

        // If we are goingLeft, we want to schedule the tiles starting from the
        // right (and to the left if not). This is because tiles are appended to
        // the list and the texture uploader goes through the set front to back.
        if (goingLeft)
            x += (tilesInRow - 1) - i;
        else
            x += i;

        if (x < 0)
            continue;

        BaseTile* currentTile = 0;
        BaseTile* availableTile = 0;
        for (int j = 0; j < m_baseTileSize; j++) {
            BaseTile& tile = m_baseTiles[j];
            if (tile.x() == x && tile.y() == y) {
                currentTile = &tile;
                break;
            }
            if (!availableTile && tile.isAvailable())
                availableTile = &tile;
        }

        if (!currentTile && availableTile) {
            currentTile = availableTile;
            currentTile->setContents(this, x, y);
        }

        if (currentTile) {
            currentTile->setScale(m_scale);

            // ensure there is a texture associated with the tile and then check to
            // see if the texture is dirty and in need of repainting
            currentTile->reserveTexture();
            updateTileUsedLevel(tileBounds, *currentTile);
            if (currentTile->isDirty() && !currentTile->isRepaintPending()) {
                PaintTileOperation *operation = new PaintTileOperation(currentTile);
                TilesManager::instance()->scheduleOperation(operation);
            } else if (currentTile->isDirty()) {
                XLOG("Tile %dx%d is dirty, but awaiting repaint", currentTile->x(), currentTile->y());
            }
        }
    }
}

void TiledPage::updateTileUsedLevel(const SkIRect& tileBounds, BaseTile& tile)
{
    const int lastTileX = tileBounds.fRight - 1;
    const int lastTileY = tileBounds.fBottom - 1;

    // set the used level of the tile (e.g. distance from the viewport)
    int dx = 0;
    int dy = 0;

    if (tileBounds.fLeft > tile.x())
        dx = tileBounds.fLeft - tile.x();
    else if (lastTileX < tile.x())
        dx = tile.x() - lastTileX;

    if (tileBounds.fTop > tile.y())
        dy = tileBounds.fTop - tile.y();
    else if (lastTileY < tile.y())
        dy = tile.y() - lastTileY;

    int d = std::max(dx, dy);

    tile.setUsedLevel(d);
}

void TiledPage::updateTileState(const SkIRect& tileBounds)
{
    if (!m_glWebViewState || tileBounds.isEmpty()) {
        m_invalRegion.setEmpty();
        m_invalTilesRegion.setEmpty();
        return;
    }

    for (int x = 0; x < m_baseTileSize; x++) {

        BaseTile& tile = m_baseTiles[x];

        // if the tile no longer has a texture then proceed to the next tile
        if (tile.isAvailable())
            continue;

        // if the tile is in the dirty region then we must invalidate it
        if (m_invalRegion.contains(tile.x(), tile.y()))
            tile.markAsDirty(m_latestPictureInval, m_invalTilesRegion);

        updateTileUsedLevel(tileBounds, tile);
    }

    // clear the invalidated region as all tiles within that region have now
    // been marked as dirty.
    m_invalRegion.setEmpty();
    m_invalTilesRegion.setEmpty();
}

void TiledPage::prepare(bool goingDown, bool goingLeft, const SkIRect& tileBounds)
{
    if (!m_glWebViewState)
        return;

    // update the tiles distance from the viewport
    updateTileState(tileBounds);
    m_prepare = true;
    m_scrollingDown = goingDown;

    int firstTileX = tileBounds.fLeft;
    int firstTileY = tileBounds.fTop;
    int nbTilesWidth = tileBounds.width();
    int nbTilesHeight = tileBounds.height();

    int lastTileX = tileBounds.fRight - 1;
    int lastTileY = tileBounds.fBottom - 1;

    const int baseContentHeight = m_glWebViewState->baseContentHeight();
    const int baseContentWidth = m_glWebViewState->baseContentWidth();

    // Expand number of tiles to allow tiles outside of viewport to be prepared for
    // smoother scrolling.
    int nTilesToPrepare = nbTilesWidth * nbTilesHeight;
    int nMaxTilesPerPage = m_baseTileSize / 2;
    int expandX = TilesManager::instance()->expandedTileBoundsX();
    int expandY = TilesManager::instance()->expandedTileBoundsY();
    if (nTilesToPrepare + (nbTilesHeight * expandX * 2) <= nMaxTilesPerPage) {
        firstTileX -= expandX;
        lastTileX += expandX;
        nbTilesWidth += expandX * 2;
    }
    if (nTilesToPrepare + (nbTilesWidth * expandY * 2) <= nMaxTilesPerPage) {
        firstTileY -= expandY;
        lastTileY += expandY;
        nbTilesHeight += expandY * 2;
    }
    m_expandedTileBounds.fLeft = firstTileX;
    m_expandedTileBounds.fTop = firstTileY;
    m_expandedTileBounds.fRight = lastTileX;
    m_expandedTileBounds.fBottom = lastTileY;

    for (int i = 0; i < nbTilesHeight; i++)
        prepareRow(goingLeft, nbTilesWidth, firstTileX, firstTileY + i, tileBounds);
}

bool TiledPage::ready(const SkIRect& tileBounds, float scale)
{
    if (!m_glWebViewState)
        return false;

    if (!m_invalRegion.isEmpty() && !m_prepare)
        return false;

    if (m_scale != scale)
        return false;

    for (int x = tileBounds.fLeft; x < tileBounds.fRight; x++) {
        for (int y = tileBounds.fTop; y < tileBounds.fBottom; y++) {
            BaseTile* t = getBaseTile(x, y);
            if (!t || !t->isTileReady())
                return false;
        }
    }
    m_prepare = false;
    return true;
}

void TiledPage::draw(float transparency, const SkIRect& tileBounds)
{
    if (!m_glWebViewState)
        return;

    const float tileWidth = TilesManager::tileWidth() * m_invScale;
    const float tileHeight = TilesManager::tileHeight() * m_invScale;

    SkIRect actualTileBounds = tileBounds;
    actualTileBounds.fTop -= TilesManager::instance()->expandedTileBoundsY();
    actualTileBounds.fBottom += TilesManager::instance()->expandedTileBoundsY();
    actualTileBounds.fLeft -= TilesManager::instance()->expandedTileBoundsX();
    actualTileBounds.fRight += TilesManager::instance()->expandedTileBoundsX();

    for (int j = 0; j < m_baseTileSize; j++) {
        BaseTile& tile = m_baseTiles[j];
        if (actualTileBounds.contains(tile.x(), tile.y())) {

            SkRect rect;
            rect.fLeft = tile.x() * tileWidth;
            rect.fTop = tile.y() * tileHeight;
            rect.fRight = rect.fLeft + tileWidth;
            rect.fBottom = rect.fTop + tileHeight;

            tile.draw(transparency, rect, m_scale);
        }
    }
}

unsigned int TiledPage::paintBaseLayerContent(SkCanvas* canvas)
{
    if (m_glWebViewState)
        return m_glWebViewState->paintBaseLayerContent(canvas);
    return 0;
}

TiledPage* TiledPage::sibling()
{
    if (!m_glWebViewState)
        return 0;
    return m_glWebViewState->sibling(this);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
