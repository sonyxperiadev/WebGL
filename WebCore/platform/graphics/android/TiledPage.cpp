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

#ifdef DEBUG_COUNT
static int gTilePageCount = 0;
int TiledPage::count()
{
    return gTilePageCount;
}
#endif

TiledPage::TiledPage(int id, GLWebViewState* state)
    : m_id(id)
    , m_scale(1)
    , m_invScale(1)
    , m_glWebViewState(state)
    , m_latestPictureInval(0)
{
    // This value must be at least 1 greater than the max number of allowed
    // textures. This is because prepare() asks for a tile before it reserves
    // a texture for that tile. If all textures are currently in use by the
    // page then there will be no available tile and having the extra tile
    // ensures that this does not happen. After claiming the extra tile the call
    // to reserveTexture() will cause some other tile in the page to lose it's
    // texture and become available, thus ensuring that we always have at least
    // one tile that is available.
    m_baseTileSize = TilesManager::maxTextureCount() + 1;
    m_baseTiles = new BaseTile[m_baseTileSize];

#ifdef DEBUG_COUNT
    gTilePageCount++;
#endif
}

TiledPage::~TiledPage() {
    // In order to delete the page we must ensure that none of its BaseTiles are
    // currently painting or scheduled to be painted by the TextureGenerator
    TilesManager::instance()->removeSetsWithPage(this);
    delete[] m_baseTiles;
#ifdef DEBUG_COUNT
    gTilePageCount--;
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

void TiledPage::invalidateRect(const IntRect& inval, const unsigned int pictureCount)
{
    // Given the current scale level we need to mark the appropriate tiles as dirty
    const float invTileContentWidth = m_scale / TilesManager::tileWidth();
    const float invTileContentHeight = m_scale / TilesManager::tileHeight();

    const int firstDirtyTileX = static_cast<int>(floorf(inval.x() * invTileContentWidth));
    const int firstDirtyTileY = static_cast<int>(floorf(inval.y() * invTileContentHeight));
    const int lastDirtyTileX = static_cast<int>(ceilf(inval.right() * invTileContentWidth));
    const int lastDirtyTileY = static_cast<int>(ceilf(inval.bottom() * invTileContentHeight));

    // We defer marking the tile as dirty until the next time we need to prepare
    // to draw.
    m_invalRegion.op(firstDirtyTileX, firstDirtyTileY, lastDirtyTileX, lastDirtyTileY, SkRegion::kUnion_Op);
    m_latestPictureInval = pictureCount;
}

void TiledPage::prepareRow(bool goingLeft, int tilesInRow, int firstTileX, int y, TileSet* set)
{
    if (y < 0)
        return;
    if (!set)
        return;

    for (int i = 0; i < tilesInRow; i++) {
        int x = firstTileX;

        // If we are goingLeft, we want to schedule the tiles
        // starting from the left (and to the right if not)
        if (goingLeft)
          x += i;
        else
          x += (tilesInRow - 1) - i;


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

        if (!currentTile) {
            currentTile = availableTile;
            currentTile->setContents(this, x, y);
        }

        currentTile->setScale(m_scale);

        // ensure there is a texture associated with the tile and then check to
        // see if the texture is dirty and in need of repainting
        currentTile->reserveTexture();
        if(currentTile->isDirty())
            set->add(currentTile);
    }
}

void TiledPage::updateTileState(int firstTileX, int firstTileY)
{
    if (!m_glWebViewState)
        return;

    const int nbTilesWidth = m_glWebViewState->nbTilesWidth();
    const int nbTilesHeight = m_glWebViewState->nbTilesHeight();

    for (int x = 0; x < m_baseTileSize; x++) {

        BaseTile& tile = m_baseTiles[x];

        // if the tile no longer has a texture then proceed to the next tile
        if (tile.isAvailable())
            continue;

        // if the tile is in the dirty region then we must invalidate it
        if (m_invalRegion.contains(tile.x(), tile.y()))
            tile.markAsDirty(m_latestPictureInval);

        // set the used level of the tile (e.g. distance from the viewport)
        int dx = 0;
        int dy = 0;

        if (firstTileX > tile.x())
            dx = firstTileX - tile.x();
        else if (firstTileX + (nbTilesWidth - 1) < tile.x())
            dx = tile.x() - firstTileX - (nbTilesWidth - 1);

        if (firstTileY > tile.y())
            dy = firstTileY - tile.y();
        else if (firstTileY + (nbTilesHeight - 1) < tile.y())
            dy = tile.y() - firstTileY - (nbTilesHeight - 1);

        int d = std::max(dx, dy);

        XLOG("setTileLevel tile: %x, fxy(%d, %d), level: %d", tile, firstTileX, firstTileY, d);
        tile.setUsedLevel(d);
    }

    // clear the invalidated region as all tiles within that region have now
    // been marked as dirty.
    m_invalRegion.setEmpty();
}

void TiledPage::prepare(bool goingDown, bool goingLeft, int firstTileX, int firstTileY)
{
    if (!m_glWebViewState)
        return;

    // update the tiles distance from the viewport
    updateTileState(firstTileX, firstTileY);

    int nbTilesWidth = m_glWebViewState->nbTilesWidth();
    int nbTilesHeight = m_glWebViewState->nbTilesHeight();

    TileSet* highResSet = new TileSet(this, nbTilesHeight, nbTilesWidth);

    // We chose to display tiles depending on the scroll direction:
    if (goingDown) {
        for (int i = 0; i < nbTilesHeight; i++)
            prepareRow(goingLeft, nbTilesWidth, firstTileX, firstTileY + i, highResSet);
    } else {
        int startingTileY = firstTileY + (nbTilesHeight - 1);
        for (int i = 0; i < nbTilesHeight; i++)
            prepareRow(goingLeft, nbTilesWidth, firstTileX, startingTileY - i, highResSet);
    }

    // schedulePaintForTileSet will take ownership of the highResSet here,
    // so no delete necessary.
    TilesManager::instance()->schedulePaintForTileSet(highResSet);
}

bool TiledPage::ready(int firstTileX, int firstTileY)
{
    if (!m_glWebViewState)
        return false;

    int nbTilesWidth = m_glWebViewState->nbTilesWidth();
    int nbTilesHeight = m_glWebViewState->nbTilesHeight();

    for (int i = 0; i < nbTilesHeight; i++) {
        for (int j = 0; j < nbTilesWidth; j++) {
            int x = j + firstTileX;
            int y = i + firstTileY;
            BaseTile* t = getBaseTile(x, y);
            if (!t || !t->isTileReady())
                return false;
        }
    }
    return true;
}

void TiledPage::draw(float transparency, SkRect& viewport, int firstTileX, int firstTileY)
{
    if (!m_glWebViewState)
        return;

    const float tileWidth = TilesManager::tileWidth() * m_invScale;
    const float tileHeight = TilesManager::tileHeight() * m_invScale;

    SkIRect viewportTilesRect;
    viewportTilesRect.fLeft = firstTileX;
    viewportTilesRect.fTop = firstTileY;
    viewportTilesRect.fRight = firstTileY + m_glWebViewState->nbTilesWidth() + 1;
    viewportTilesRect.fBottom = firstTileY + m_glWebViewState->nbTilesHeight() + 1;

    XLOG("WE DRAW %x (%.2f) with transparency %.2f", this, scale(), transparency);
    for (int j = 0; j < m_baseTileSize; j++) {
        BaseTile& tile = m_baseTiles[j];
        if(viewportTilesRect.contains(tile.x(), tile.y())) {

            SkRect rect;
            rect.fLeft = tile.x() * tileWidth;
            rect.fTop = tile.y() * tileHeight;
            rect.fRight = rect.fLeft + tileWidth;
            rect.fBottom = rect.fTop + tileHeight;

            TilesManager::instance()->shader()->setViewport(viewport);
            tile.draw(transparency, rect);
        }
    }

#ifdef DEBUG
    XLOG("FINISHED WE DRAW %x (%.2f) with transparency %.2f", this, scale(), transparency);
    TilesManager::instance()->printTextures();
#endif // DEBUG
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
    return (m_glWebViewState->frontPage() == this) ? m_glWebViewState->backPage() : this;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
