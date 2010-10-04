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

TiledPage::TiledPage(int id, GLWebViewState* state)
    : m_id(id)
    , m_scale(1)
    , m_glWebViewState(state)
{
}

BaseTile* TiledPage::getBaseTile(int x, int y, int quality)
{
    TileKey key(x + 1, y + 1);
    return m_baseTextures.get(key);
}

void TiledPage::prepareRow(bool goingLeft, int firstTileX, int y, TilesSet* set, int quality)
{
    if (y < 0)
        return;
    if (!set)
        return;

    int nbCols = set->nbCols();
    for (int i = 0; i < nbCols; i++) {
        int x = firstTileX;

        // If we are goingLeft, we want to schedule the tiles
        // starting from the left (and to the right if not)
        if (goingLeft)
          x += i;
        else
          x += (nbCols - 1) - i;

        TileKey key(x + 1, y + 1);
        BaseTile* tile = 0;
        if (!m_baseTextures.contains(key)) {
            tile = new BaseTile(this, x, y);
            m_baseTextures.set(key, tile);
        }
        tile = m_baseTextures.get(key);
        tile->setUsedLevel(0);
        tile->setScale(m_scale);
        set->add(tile);
    }
}

void TiledPage::setTileLevel(BaseTile* tile, int firstTileX, int firstTileY)
{
    if (!tile)
        return;

    if (!m_glWebViewState)
        return;

    int nbTilesWidth = m_glWebViewState->nbTilesWidth();
    int nbTilesHeight = m_glWebViewState->nbTilesHeight();
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

void TiledPage::prepare(bool goingDown, bool goingLeft, int firstTileX, int firstTileY)
{
    if (!m_glWebViewState)
        return;

    int nbTilesWidth = m_glWebViewState->nbTilesWidth();
    int nbTilesHeight = m_glWebViewState->nbTilesHeight();

    TilesSet* highResSet = new TilesSet(m_id + 1, scale(), firstTileX, firstTileY,
                                       nbTilesHeight, nbTilesWidth);

    // We chose to display tiles depending on the scroll direction:
    if (goingDown) {
        for (int i = 0; i < nbTilesHeight; i++)
            prepareRow(goingLeft, firstTileX, firstTileY + i, highResSet, 1);
    } else {
        for (int i = 0; i < nbTilesHeight; i++)
            prepareRow(goingLeft, firstTileX, firstTileY + (nbTilesHeight - 1) - i, highResSet, 1);
    }

    TileMap::const_iterator end = m_baseTextures.end();
    for (TileMap::const_iterator it = m_baseTextures.begin(); it != end; ++it) {
        BaseTile* tile = it->second;
        setTileLevel(tile, firstTileX, firstTileY);
    }

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

    // schedulePaintForTilesSet will take ownership of the highResSet here,
    // so no delete necessary.
    TilesManager::instance()->schedulePaintForTilesSet(highResSet);
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
            BaseTile* t = getBaseTile(x, y, 1);
            if (!t || !t->isBitmapReady())
                return false;
        }
    }
    return true;
}

void TiledPage::draw(float transparency, SkRect& viewport,
                     int firstTileX, int firstTileY)
{
    if (!m_glWebViewState)
        return;

    float w = TilesManager::instance()->tileWidth() / m_scale;
    float h = TilesManager::instance()->tileHeight() / m_scale;
    int nbTilesWidth = m_glWebViewState->nbTilesWidth();
    int nbTilesHeight = m_glWebViewState->nbTilesHeight();

    XLOG("WE DRAW %x (%.2f) with transparency %.2f", this, scale(), transparency);
    for (int i = 0; i < nbTilesHeight; i++) {
        for (int j = 0; j < nbTilesWidth; j++) {
            int x = j + firstTileX;
            int y = i + firstTileY;

            BaseTile* tile = getBaseTile(x, y, 1);

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

    XLOG("FINISHED WE DRAW %x (%.2f) with transparency %.2f", this, scale(), transparency);
    TilesManager::instance()->printTextures();
}

bool TiledPage::paintBaseLayerContent(SkCanvas* canvas)
{
    if (m_glWebViewState)
        return m_glWebViewState->paintBaseLayerContent(canvas);
    return false;
}

unsigned int TiledPage::currentPictureCounter()
{
    if (m_glWebViewState)
        return m_glWebViewState->currentPictureCounter();
    return 0;
}

TiledPage* TiledPage::sibling()
{
    if (!m_glWebViewState)
        return 0;

    if (m_glWebViewState->frontPage() == this)
        return m_glWebViewState->backPage();
    return m_glWebViewState->frontPage();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
