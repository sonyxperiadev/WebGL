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

#ifndef BaseTile_h
#define BaseTile_h

#if USE(ACCELERATED_COMPOSITING)

#include "BaseRenderer.h"
#include "SkRect.h"
#include "SkRegion.h"
#include "TextureOwner.h"
#include "TilePainter.h"

#include <utils/threads.h>

namespace WebCore {

class TextureInfo;
class TiledPage;
class BaseTileTexture;
class GLWebViewState;

/**
 * An individual tile that is used to construct part of a webpage's BaseLayer of
 * content.  Each tile is assigned to a TiledPage and is responsible for drawing
 * and displaying their section of the page.  The lifecycle of a tile is:
 *
 * 1. Each tile is created on the main GL thread and assigned to a specific
 *    location within a TiledPage.
 * 2. When needed the tile is passed to the background thread where it paints
 *    the BaseLayer's most recent PictureSet to a bitmap which is then uploaded
 *    to the GPU.
 * 3. After the bitmap is uploaded to the GPU the main GL thread then uses the
 *    tile's draw() function to display the tile to the screen.
 * 4. Steps 2-3 are repeated as necessary.
 * 5. The tile is destroyed when the user navigates to a new page.
 *
 */
class BaseTile : public TextureOwner {
public:

    // eventually, m_dirty might be rolled into the state machine, but note
    // that a tile that's continually marked dirty from animation should still
    // progress through the state machine and be drawn periodically (esp. for
    // layers)

    //                                /->  TransferredUnvalidated (TQ interrupts paint)    -\   (TQ & paint done)
    // Unpainted -> PaintingStarted --                                                       ->    ReadyToSwap    -> UpToDate
    //     ^                          \->  ValidatedUntransferred (paint finish before TQ) -/
    //     |
    //     \--... (From any state when marked dirty. should usually come from UpToDate if the updates are locked)
    //

    enum TextureState{
        // back texture is completely unpainted
        Unpainted = 0,
        // has started painting, but haven't been transferred or validated
        PaintingStarted = 1,
        // back texture painted, transferred before validating in PaintBitmap()
        TransferredUnvalidated = 2,
        // back texture painted, validated before transferring in TransferQueue
        ValidatedUntransferred = 3,
        // back texture has been blitted, will be swapped when next available
        ReadyToSwap = 4,
        // has been swapped, is ready to draw, all is well
        UpToDate = 5,
    };

    BaseTile(bool isLayerTile = false);
    ~BaseTile();

    bool isLayerTile() { return m_isLayerTile; }

    void setContents(TilePainter* painter, int x, int y, float scale);
    void setPage(TiledPage* page) { m_page = page; }

    void reserveTexture();

    bool isTileReady();

    void draw(float transparency, SkRect& rect, float scale);

    // the only thread-safe function called by the background thread
    void paintBitmap();

    bool intersectWithRect(int x, int y, int tileWidth, int tileHeight,
                           float scale, const SkRect& dirtyRect,
                           SkRect& realTileRect);
    bool isTileVisible(const IntRect& viewTileBounds);

    void markAsDirty(const unsigned int pictureCount,
                     const SkRegion& dirtyArea);
    bool isDirty();
    bool isRepaintPending();
    void setRepaintPending(bool pending);
    float scale() const { return m_scale; }
    TextureState textureState() const { return m_state; }

    int x() const { return m_x; }
    int y() const { return m_y; }
    BaseTileTexture* frontTexture() { return m_frontTexture; }
    BaseTileTexture* backTexture() { return m_backTexture; }

    // only used for prioritization - the higher, the more relevant the tile is
    unsigned long long drawCount() { return m_drawCount; }
    void discardTextures();
    void discardBackTexture();
    bool swapTexturesIfNeeded();
    void backTextureTransfer();
    void backTextureTransferFail();

    void setGLWebViewState(GLWebViewState* state) { m_glWebViewState = state; }

    // TextureOwner implementation
    virtual bool removeTexture(BaseTileTexture* texture);
    virtual TiledPage* page() { return m_page; }
    virtual GLWebViewState* state() { return m_glWebViewState; }
    TilePainter* painter() { return m_painter; }

private:
    void validatePaint();

    GLWebViewState* m_glWebViewState;

    TilePainter* m_painter;
    int m_x;
    int m_y;

    TiledPage* m_page;

    // The remaining variables can be updated throughout the lifetime of the object

    BaseTileTexture* m_frontTexture;
    BaseTileTexture* m_backTexture;
    float m_scale;

    // used to signal that the that the tile is out-of-date and needs to be
    // redrawn in the backTexture
    bool m_dirty;

    // currently only for debugging, to be used for tracking down dropped repaints
    bool m_deferredDirty;

    // used to signal that a repaint is pending
    bool m_repaintPending;
    // stores the id of the latest picture from webkit that caused this tile to
    // become dirty. A tile is no longer dirty when it has been painted with a
    // picture that is newer than this value.
    unsigned int m_lastDirtyPicture;

    // store the dirty region
    SkRegion* m_dirtyArea;
    bool* m_fullRepaint;
    int m_maxBufferNumber;
    int m_currentDirtyAreaIndex;

    // flag used to know if we have a texture that was painted at least once
    bool m_isTexturePainted;

    // This mutex serves two purposes. (1) It ensures that certain operations
    // happen atomically and (2) it makes sure those operations are synchronized
    // across all threads and cores.
    android::Mutex m_atomicSync;

    BaseRenderer* m_renderer;

    bool m_isLayerTile;

    // the most recent GL draw before this tile was prepared. used for
    // prioritization and caching. tiles with old drawcounts and textures they
    // own are used for new tiles and rendering
    unsigned long long m_drawCount;

    // Tracks the state of painting for the tile. High level overview:
    // 1) Unpainted - until paint starts (and if marked dirty, in most cases)
    // 2) PaintingStarted - until paint completes
    // 3) TransferredUnvalidated - if transferred first
    //    or ValidatedUntransferred - if validated first
    // 4) ReadyToSwap - if painted and transferred, but not swapped
    // 5) UpToDate - until marked dirty again
    TextureState m_state;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // BaseTile_h
