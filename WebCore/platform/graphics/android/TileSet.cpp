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
#include "TileSet.h"

#if USE(ACCELERATED_COMPOSITING)

#include "TilesManager.h"

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "TileSet", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

#ifdef DEBUG_COUNT
static int gTileSetCount = 0;
int TileSet::count()
{
    return gTileSetCount;
}
#endif

TileSet::TileSet(int id, int firstTileX, int firstTileY, int rows, int cols)
    : m_id(id)
    , m_firstTileX(firstTileX)
    , m_firstTileY(firstTileY)
    , m_nbRows(rows)
    , m_nbCols(cols)
{
#ifdef DEBUG_COUNT
    gTileSetCount++;
#endif
}

TileSet::~TileSet()
{
#ifdef DEBUG_COUNT
    gTileSetCount--;
#endif
}

bool TileSet::operator==(const TileSet& set)
{
    return m_id == set.m_id
           && m_firstTileX == set.m_firstTileX
           && m_firstTileY == set.m_firstTileY
           && m_nbRows == set.m_nbRows
           && m_nbCols == set.m_nbCols;
}


void TileSet::reserveTextures()
{
#ifdef DEBUG
    if (m_tiles.size()) {
        TiledPage* page = m_tiles[0]->page();
        XLOG("reserveTextures (%d tiles) for page %x (sibling: %x)", m_tiles.size(), page, page->sibling());
        TilesManager::instance()->printTextures();
    }
#endif // DEBUG

    for (unsigned int i = 0; i < m_tiles.size(); i++)
        m_tiles[i]->reserveTexture();

#ifdef DEBUG
    if (m_tiles.size()) {
        TiledPage* page = m_tiles[0]->page();
        XLOG(" DONE reserveTextures (%d tiles) for page %x (sibling: %x)", m_tiles.size(), page, page->sibling());
        TilesManager::instance()->printTextures();
    }
#endif // DEBUG
}

void TileSet::paint()
{
    XLOG("%x, painting %d tiles", this, m_tiles.size());
    for (unsigned int i = 0; i < m_tiles.size(); i++)
        m_tiles[i]->paintBitmap();
    XLOG("%x, end of painting %d tiles", this, m_tiles.size());
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
