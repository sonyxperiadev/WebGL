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
#include "GLWebViewState.h"
#include "SkCanvas.h"
#include "TileSet.h"

namespace WebCore {

class TiledPage {
public:
    TiledPage(int id, GLWebViewState* state);
    ~TiledPage() { }
    BaseTile* getBaseTile(int x, int y);
    void prepare(bool goingDown, bool goingLeft, int firstTileX, int firstTileY);
    void setScale(float scale) { m_scale = scale; }
    bool ready(int firstTileX, int firstTileY);
    void draw(float transparency, SkRect& viewport, int firstTileX, int firstTileY);
    float scale() const { return m_scale; }
    bool paintBaseLayerContent(SkCanvas*);
    unsigned int currentPictureCounter();
    TiledPage* sibling();
    GLWebViewState* glWebViewState() { return m_glWebViewState; }
private:
    void setTileLevel(BaseTile* tile, int firstTileX, int firstTileY);
    void prepareRow(bool goingLeft, int tilesInRow, int firstTileX, int y, TileSet* set);
    TileMap m_baseTextures;
    int m_id;
    float m_scale;
    GLWebViewState* m_glWebViewState;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TiledPage_h
