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
#include "SkPaint.h"
#include "SkPaintFlagsDrawFilter.h"
#include "TilesManager.h"

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "TiledPage", __VA_ARGS__)

#ifdef DEBUG

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
    , m_isPrefetchPage(false)
    , m_willDraw(false)
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
    // TODO: replace loop over array with HashMap indexing
    for (int j = 0; j < m_baseTileSize; j++) {
        BaseTile& tile = m_baseTiles[j];
        if (tile.x() == x && tile.y() == y)
            return &tile;
    }
    return 0;
}

void TiledPage::discardTextures()
{
    for (int j = 0; j < m_baseTileSize; j++) {
        BaseTile& tile = m_baseTiles[j];
        tile.discardTextures();
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
    const int lastDirtyTileX = static_cast<int>(ceilf(inval.maxX() * invTileContentWidth));
    const int lastDirtyTileY = static_cast<int>(ceilf(inval.maxY() * invTileContentHeight));

    XLOG("Marking X %d-%d and Y %d-%d dirty", firstDirtyTileX, lastDirtyTileX, firstDirtyTileY, lastDirtyTileY);
    // We defer marking the tile as dirty until the next time we need to prepare
    // to draw.
    m_invalRegion.op(firstDirtyTileX, firstDirtyTileY, lastDirtyTileX, lastDirtyTileY, SkRegion::kUnion_Op);
    m_invalTilesRegion.op(inval.x(), inval.y(), inval.maxX(), inval.maxY(), SkRegion::kUnion_Op);
    m_latestPictureInval = pictureCount;
}

void TiledPage::prepareRow(bool goingLeft, int tilesInRow, int firstTileX, int y, const SkIRect& tileBounds)
{
    for (int i = 0; i < tilesInRow; i++) {
        int x = firstTileX;

        // If we are goingLeft, we want to schedule the tiles starting from the
        // right (and to the left if not). This is because tiles are appended to
        // the list and the texture uploader goes through the set front to back.
        if (goingLeft)
            x += (tilesInRow - 1) - i;
        else
            x += i;

        BaseTile* currentTile = 0;
        BaseTile* availableTile = 0;
        for (int j = 0; j < m_baseTileSize; j++) {
            BaseTile& tile = m_baseTiles[j];
            if (tile.x() == x && tile.y() == y) {
                currentTile = &tile;
                break;
            }

            if (!availableTile || (tile.drawCount() < availableTile->drawCount()))
                availableTile = &tile;
        }

        if (!currentTile && availableTile) {
            XLOG("STEALING tile %d, %d (draw count %llu) for tile %d, %d",
                  availableTile->x(), availableTile->y(), availableTile->drawCount(), x, y);
            availableTile->discardTextures(); // don't wait for textures to be stolen
            currentTile = availableTile;
        }

        if (!currentTile) {
            XLOG("ERROR: No tile available for tile %d %d", x, y);
        }

        if (currentTile) {
            currentTile->setGLWebViewState(m_glWebViewState);
            currentTile->setPage(this);

            currentTile->setContents(this, x, y, m_scale);

            // TODO: move below (which is largely the same for layers / tiled
            // page) into prepare() function

            // ensure there is a texture associated with the tile and then check to
            // see if the texture is dirty and in need of repainting
            if (currentTile->isDirty() || !currentTile->frontTexture())
                currentTile->reserveTexture();
            if (currentTile->backTexture()
                    && currentTile->isDirty()
                    && !currentTile->isRepaintPending()) {
                PaintTileOperation *operation = new PaintTileOperation(currentTile);
                TilesManager::instance()->scheduleOperation(operation);
            }
        }
    }
}

bool TiledPage::updateTileDirtiness(const SkIRect& tileBounds)
{
    if (!m_glWebViewState || tileBounds.isEmpty()) {
        m_invalRegion.setEmpty();
        m_invalTilesRegion.setEmpty();
        return false;
    }

    bool visibleTileIsDirty = false;
    for (int x = 0; x < m_baseTileSize; x++) {

        BaseTile& tile = m_baseTiles[x];

        // if the tile is in the dirty region then we must invalidate it
        if (m_invalRegion.contains(tile.x(), tile.y())) {
            tile.markAsDirty(m_latestPictureInval, m_invalTilesRegion);
            if (tileBounds.contains(tile.x(), tile.y()))
                visibleTileIsDirty = true;
        }
    }

    // clear the invalidated region as all tiles within that region have now
    // been marked as dirty.
    m_invalRegion.setEmpty();
    m_invalTilesRegion.setEmpty();
    return visibleTileIsDirty;
}

void TiledPage::prepare(bool goingDown, bool goingLeft, const SkIRect& tileBounds, PrepareBounds bounds)
{
    if (!m_glWebViewState)
        return;

    TilesManager::instance()->gatherTextures();
    m_scrollingDown = goingDown;

    int firstTileX = tileBounds.fLeft;
    int firstTileY = tileBounds.fTop;
    int nbTilesWidth = tileBounds.width();
    int nbTilesHeight = tileBounds.height();

    // Expand number of tiles to allow tiles outside of viewport to be prepared for
    // smoother scrolling.
    int nTilesToPrepare = nbTilesWidth * nbTilesHeight;
    int nMaxTilesPerPage = m_baseTileSize / 2;

    if (bounds == ExpandedBounds) {
        // prepare tiles outside of the visible bounds
        int expandX = m_glWebViewState->expandedTileBoundsX();
        int expandY = m_glWebViewState->expandedTileBoundsY();

        firstTileX -= expandX;
        nbTilesWidth += expandX * 2;

        firstTileY -= expandY;
        nbTilesHeight += expandY * 2;
    }

    // crop the tile bounds in each dimension to the larger of the base layer or viewport
    float maxBaseX = m_glWebViewState->baseContentWidth() * m_scale / TilesManager::tileWidth();
    float maxBaseY = m_glWebViewState->baseContentHeight() * m_scale / TilesManager::tileHeight();
    int maxX = std::max(static_cast<int>(ceilf(maxBaseX)),
                        m_glWebViewState->viewportTileBounds().width());
    int maxY = std::max(static_cast<int>(ceilf(maxBaseY)),
                        m_glWebViewState->viewportTileBounds().height());

    // adjust perimeter to not go outside cropped region
    if (firstTileX < 0) {
        nbTilesWidth += firstTileX;
        firstTileX = 0;
    }
    if (firstTileY < 0) {
        nbTilesHeight += firstTileY;
        firstTileY = 0;
    }
    nbTilesWidth = std::min(nbTilesWidth, maxX - firstTileX);
    nbTilesHeight = std::min(nbTilesHeight, maxY - firstTileY);

    // check against corrupted scale values giving bad height/width (use float to avoid overflow)
    float numTiles = static_cast<float>(nbTilesHeight) * static_cast<float>(nbTilesWidth);
    if (numTiles > TilesManager::getMaxTextureAllocation() || nbTilesHeight < 1 || nbTilesWidth < 1)
    {
        XLOGC("ERROR: We don't have enough tiles for this page!"
              " nbTilesHeight %d nbTilesWidth %d", nbTilesHeight, nbTilesWidth);
        return;
    }
    for (int i = 0; i < nbTilesHeight; i++)
        prepareRow(goingLeft, nbTilesWidth, firstTileX, firstTileY + i, tileBounds);

    m_prepare = true;
}

bool TiledPage::hasMissingContent(const SkIRect& tileBounds)
{
    int neededTiles = tileBounds.width() * tileBounds.height();
    for (int j = 0; j < m_baseTileSize; j++) {
        BaseTile& tile = m_baseTiles[j];
        if (tileBounds.contains(tile.x(), tile.y())) {
            if (tile.frontTexture())
                neededTiles--;
        }
    }
    return neededTiles > 0;
}

bool TiledPage::isReady(const SkIRect& tileBounds)
{
    int neededTiles = tileBounds.width() * tileBounds.height();
    XLOG("tiled page %p needs %d ready tiles", this, neededTiles);
    for (int j = 0; j < m_baseTileSize; j++) {
        BaseTile& tile = m_baseTiles[j];
        if (tileBounds.contains(tile.x(), tile.y())) {
            if (tile.isTileReady())
                neededTiles--;
        }
    }
    XLOG("tiled page %p still needs %d ready tiles", this, neededTiles);
    return neededTiles == 0;
}

bool TiledPage::swapBuffersIfReady(const SkIRect& tileBounds, float scale)
{
    if (!m_glWebViewState)
        return false;

    if (!m_invalRegion.isEmpty() && !m_prepare)
        return false;

    if (m_scale != scale)
        return false;

    int swaps = 0;
    bool fullSwap = true;
    for (int x = tileBounds.fLeft; x < tileBounds.fRight; x++) {
        for (int y = tileBounds.fTop; y < tileBounds.fBottom; y++) {
            BaseTile* t = getBaseTile(x, y);
            if (!t || !t->isTileReady())
                fullSwap = false;
        }
    }

    // swap every tile on page (even if off screen)
    for (int j = 0; j < m_baseTileSize; j++) {
        BaseTile& tile = m_baseTiles[j];
        if (tile.swapTexturesIfNeeded())
            swaps++;
    }

    XLOG("%p greedy swapped %d textures, returning true", this, swaps);
    return fullSwap;
}

void TiledPage::prepareForDrawGL(float transparency, const SkIRect& tileBounds)
{
    m_willDraw = true;
    m_transparency = transparency;
    m_tileBounds = tileBounds;
}

void TiledPage::drawGL()
{
    if (!m_glWebViewState || m_transparency == 0 || !m_willDraw)
        return;

    const float tileWidth = TilesManager::tileWidth() * m_invScale;
    const float tileHeight = TilesManager::tileHeight() * m_invScale;

    for (int j = 0; j < m_baseTileSize; j++) {
        BaseTile& tile = m_baseTiles[j];
        bool tileInView = m_tileBounds.contains(tile.x(), tile.y());
        if (tileInView) {
            SkRect rect;
            rect.fLeft = tile.x() * tileWidth;
            rect.fTop = tile.y() * tileHeight;
            rect.fRight = rect.fLeft + tileWidth;
            rect.fBottom = rect.fTop + tileHeight;

            tile.draw(m_transparency, rect, m_scale);
        }

        TilesManager::instance()->getProfiler()->nextTile(tile, m_invScale, tileInView);
    }
    m_willDraw = false; // don't redraw until re-prepared
}

bool TiledPage::paint(BaseTile* tile, SkCanvas* canvas, unsigned int* pictureUsed)
{
    static SkPaintFlagsDrawFilter prefetchFilter(SkPaint::kAllFlags,
                                                 SkPaint::kAntiAlias_Flag);

    if (!m_glWebViewState)
        return false;

    if (isPrefetchPage())
        canvas->setDrawFilter(&prefetchFilter);

    *pictureUsed = m_glWebViewState->paintBaseLayerContent(canvas);
    return true;
}

TiledPage* TiledPage::sibling()
{
    if (!m_glWebViewState)
        return 0;
    return m_glWebViewState->sibling(this);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
