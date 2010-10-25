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
{
#ifdef DEBUG_COUNT
    gTilePageCount++;
#endif
}

TiledPage::~TiledPage() {
    // Stop any pixmap generation
    if (m_baseTiles.size()) {
        TilesManager::instance()->removeSetsWithPage(this);
    }
    m_glWebViewState = 0;
    // At this point, we can safely deallocate the BaseTiles, as
    // there is no more BaseTile painting or scheduled to be painted
    // by the TextureGenerator, and as we did reset the BaseLayer in GLWebViewState,
    // in WebView's destructor (so no additional painting can be scheduled)
    deleteAllValues(m_baseTiles);
#ifdef DEBUG_COUNT
    gTilePageCount--;
#endif
}

BaseTile* TiledPage::getBaseTile(int x, int y)
{
    // if (x,y) is (0,0) the HashMap will treat the key as a null value and will
    // not store the tile so we increment the key values by 1
    TileKey key(x+1, y+1);
    return m_baseTiles.get(key);
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

        TileKey key(x+1, y+1);
        BaseTile* tile = 0;
        if (!m_baseTiles.contains(key)) {
            tile = new BaseTile(this, x, y);
            m_baseTiles.set(key, tile);
        }
        tile = m_baseTiles.get(key);
        tile->setScale(m_scale);
        set->add(tile);
    }
}

void TiledPage::setTileLevels(int firstTileX, int firstTileY)
{
    if (!m_glWebViewState)
        return;

    const int nbTilesWidth = m_glWebViewState->nbTilesWidth();
    const int nbTilesHeight = m_glWebViewState->nbTilesHeight();

    TileMap::const_iterator end = m_baseTiles.end();
    for (TileMap::const_iterator it = m_baseTiles.begin(); it != end; ++it) {
        BaseTile* tile = it->second;

        if(!tile)
            continue;

        int dx = 0;
        int dy = 0;

        if (firstTileX > tile->x())
            dx = firstTileX - tile->x();
        else if (firstTileX + (nbTilesWidth - 1) < tile->x())
            dx = tile->x() - firstTileX - (nbTilesWidth - 1);

        if (firstTileY > tile->y())
            dy = firstTileY - tile->y();
        else if (firstTileY + (nbTilesHeight - 1) < tile->y())
            dy = tile->y() - firstTileY - (nbTilesHeight - 1);

        int d = std::max(dx, dy);

        XLOG("setTileLevel tile: %x, fxy(%d, %d), level: %d", tile, firstTileX, firstTileY, d);
        tile->setUsedLevel(d);
    }
}

void TiledPage::prepare(bool goingDown, bool goingLeft, int firstTileX, int firstTileY)
{
    if (!m_glWebViewState)
        return;

    int nbTilesWidth = m_glWebViewState->nbTilesWidth();
    int nbTilesHeight = m_glWebViewState->nbTilesHeight();

    TileSet* highResSet = new TileSet(m_id, firstTileX, firstTileY, nbTilesHeight, nbTilesWidth);

    // We chose to display tiles depending on the scroll direction:
    if (goingDown) {
        for (int i = 0; i < nbTilesHeight; i++)
            prepareRow(goingLeft, nbTilesWidth, firstTileX, firstTileY + i, highResSet);
    } else {
        int startingTileY = firstTileY + (nbTilesHeight - 1);
        for (int i = 0; i < nbTilesHeight; i++)
            prepareRow(goingLeft, nbTilesWidth, firstTileX, startingTileY - i, highResSet);
    }

    // update the tiles distance from the viewport
    setTileLevels(firstTileX, firstTileY);


#ifdef DEBUG
    XLOG("+++ BEFORE RESERVE TEXTURES (%d x %d) at (%d, %d), TiledPage %x",
         nbTilesWidth, nbTilesHeight, firstTileX, firstTileY, this);
    TilesManager::instance()->printTextures();
#endif // DEBUG
    highResSet->reserveTextures();

#ifdef DEBUG
    TilesManager::instance()->printTextures();
    XLOG("--- AFTER RESERVE TEXTURES (%d x %d) at (%d, %d), TiledPage %x",
         nbTilesWidth, nbTilesHeight, firstTileX, firstTileY, this);
#endif // DEBUG

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
            if (!t || !t->isBitmapReady())
                return false;
        }
    }
    return true;
}

void TiledPage::draw(float transparency, SkRect& viewport, int firstTileX, int firstTileY)
{
    if (!m_glWebViewState)
        return;

    float w = TilesManager::instance()->tileWidth() * m_invScale;
    float h = TilesManager::instance()->tileHeight() * m_invScale;
    int nbTilesWidth = m_glWebViewState->nbTilesWidth();
    int nbTilesHeight = m_glWebViewState->nbTilesHeight();

    XLOG("WE DRAW %x (%.2f) with transparency %.2f", this, scale(), transparency);
    for (int i = 0; i < nbTilesHeight; i++) {
        for (int j = 0; j < nbTilesWidth; j++) {
            int x = j + firstTileX;
            int y = i + firstTileY;

            BaseTile* tile = getBaseTile(x, y);

            if (!tile) {
                XLOG("NO TILE AT %d, %d", x, y);
                continue;
            }

            SkRect rect;
            rect.fLeft = x * w;
            rect.fTop = y * h;
            rect.fRight = rect.fLeft + w;
            rect.fBottom = rect.fTop + h;

            TilesManager::instance()->shader()->setViewport(viewport);
            tile->draw(transparency, rect);
        }
    }

#ifdef DEBUG
    XLOG("FINISHED WE DRAW %x (%.2f) with transparency %.2f", this, scale(), transparency);
    TilesManager::instance()->printTextures();
#endif // DEBUG
}

bool TiledPage::paintBaseLayerContent(SkCanvas* canvas)
{
    if (m_glWebViewState)
        return m_glWebViewState->paintBaseLayerContent(canvas);
    return false;
}

TiledPage* TiledPage::sibling()
{
    if (!m_glWebViewState)
        return 0;
    return (m_glWebViewState->frontPage() == this) ? m_glWebViewState->backPage() : this;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
