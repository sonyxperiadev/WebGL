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

#ifndef TiledPage_h
#define TiledPage_h

#if USE(ACCELERATED_COMPOSITING)

#include "BaseTile.h"
#include "SkCanvas.h"
#include "SkRegion.h"

namespace WebCore {

class GLWebViewState;
class IntRect;

/**
 * The TiledPage represents a map of BaseTiles covering the viewport. Each
 * GLWebViewState contains two TiledPages, one to display the page at the
 * current scale factor, and another in the background that we use to paint the
 * page at a different scale factor.  For instance, when we zoom using one
 * TiledPage its tiles are scaled in hardware and therefore are subject to a
 * loss of quality. To address this when the user finishes zooming we paint the
 * background TilePage at the new scale factor.  When the background TilePage is
 * ready, we swap it with the currently displaying TiledPage.
 */
class TiledPage {
public:
    TiledPage(int id, GLWebViewState* state);
    ~TiledPage();

    // returns the other TiledPage who shares the same GLWebViewState
    TiledPage* sibling();

    // prepare the page for display on the screen
    void prepare(bool goingDown, bool goingLeft, const SkIRect& tileBounds);
    // check to see if the page is ready for display
    bool ready(const SkIRect& tileBounds, float scale);
    // draw the page on the screen
    void draw(float transparency, const SkIRect& tileBounds);

    // used by individual tiles to generate the bitmap for their tile
    unsigned int paintBaseLayerContent(SkCanvas*);
    // used by individual tiles to get the information about the current picture
    GLWebViewState* glWebViewState() { return m_glWebViewState; }

    float scale() const { return m_scale; }
    void setScale(float scale) { m_scale = scale; m_invScale = 1 / scale; }

    void invalidateRect(const IntRect& invalRect, const unsigned int pictureCount);
    void setUsable(bool usable);
    void updateBaseTileSize();
    bool scrollingDown() { return m_scrollingDown; }
    SkIRect* expandedTileBounds() { return &m_expandedTileBounds; }

private:
    void updateTileState(const SkIRect& tileBounds);
    void prepareRow(bool goingLeft, int tilesInRow, int firstTileX, int y, const SkIRect& tileBounds);
    void updateTileUsedLevel(const SkIRect& tileBounds, BaseTile& tile);

    BaseTile* getBaseTile(int x, int y) const;

    // array of tiles used to compose a page. The tiles are allocated in the
    // constructor to prevent them from potentially being allocated on the stack
    BaseTile* m_baseTiles;
    // stores the number of tiles in the m_baseTiles array. This enables us to
    // quickly iterate over the array without have to check it's size
    int m_baseTileSize;
    int m_id;
    float m_scale;
    float m_invScale;
    GLWebViewState* m_glWebViewState;

    // used to identify the tiles that have been invalidated (marked dirty) since
    // the last time updateTileState() has been called. The region is stored in
    // terms of the (x,y) coordinates used to determine the location of the tile
    // within the page, not in content/view pixel coordinates.
    SkRegion m_invalRegion;

    // inval regions in content coordinates
    SkRegion m_invalTilesRegion;
    unsigned int m_latestPictureInval;
    bool m_prepare;
    bool m_scrollingDown;
    SkIRect m_expandedTileBounds;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TiledPage_h
