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
#include "TileSet.h"

namespace WebCore {

class GLWebViewState;

typedef std::pair<int, int> TileKey;
typedef HashMap<TileKey, BaseTile*> TileMap;

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

    // returns the other TiledPage who shares the same GLWebViewState
    TiledPage* sibling();

    // prepare the page for display on the screen
    void prepare(bool goingDown, bool goingLeft, int firstTileX, int firstTileY);
    // check to see if the page is ready for display
    bool ready(int firstTileX, int firstTileY);
    // draw the page on the screen
    void draw(float transparency, SkRect& viewport, int firstTileX, int firstTileY);

    // used by individual tiles to generate the bitmap for their tile
    bool paintBaseLayerContent(SkCanvas*);
    // used by individual tiles to get the information about the current picture
    GLWebViewState* glWebViewState() { return m_glWebViewState; }

    float scale() const { return m_scale; }
    void setScale(float scale) { m_scale = scale; m_invScale = 1 / scale; }

private:
    void setTileLevels(int firstTileX, int firstTileY);
    void prepareRow(bool goingLeft, int tilesInRow, int firstTileX, int y, TileSet* set);

    BaseTile* getBaseTile(int x, int y);

    TileMap m_baseTiles;
    int m_id;
    float m_scale;
    float m_invScale;
    GLWebViewState* m_glWebViewState;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TiledPage_h
