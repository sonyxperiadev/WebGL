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

#include "HashMap.h"
#include "SharedTexture.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkRect.h"
#include "SkRegion.h"
#include "TextureOwner.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <utils/threads.h>

namespace WebCore {

class TiledPage;
class BackedDoubleBufferedTexture;

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
    BaseTile();
    ~BaseTile();

    void setContents(TiledPage* page, int x, int y);
    bool isAvailable() const { return !m_texture; }

    void reserveTexture();
    void setUsedLevel(int);
    int usedLevel();
    bool isTileReady();
    void draw(float transparency, SkRect& rect, float scale);

    // the only thread-safe function called by the background thread
    void paintBitmap();
    int paintPartialBitmap(SkIRect rect, float tx, float ty,
                            float scale, BackedDoubleBufferedTexture* texture,
                            TextureInfo* textureInfo,
                            TiledPage* tiledPage,
                            bool fullRepaint = false);
    void drawTileInfo(SkCanvas* canvas,
                      BackedDoubleBufferedTexture* texture,
                      int x, int y, float scale, int pictureCount);

    void markAsDirty(const unsigned int pictureCount,
                     const SkRegion& dirtyArea);
    bool isDirty();
    bool isRepaintPending();
    void setRepaintPending(bool pending);
    void setUsable(bool usable);
    float scale() const { return m_scale; }
    void setScale(float scale);
    void fullInval();

    int x() const { return m_x; }
    int y() const { return m_y; }
    unsigned int lastPaintedPicture() const { return m_lastPaintedPicture; }
    BackedDoubleBufferedTexture* texture() { return m_texture; }

    // TextureOwner implementation
    virtual bool removeTexture(BackedDoubleBufferedTexture* texture);
    virtual TiledPage* page() { return m_page; }

private:
    // these variables are only set when the object is constructed
    TiledPage* m_page;
    int m_x;
    int m_y;

    // The remaining variables can be updated throughout the lifetime of the object
    BackedDoubleBufferedTexture* m_texture;
    float m_scale;
    // used to signal that the that the tile is out-of-date and needs to be redrawn
    bool m_dirty;
    // used to signal that a repaint is pending
    bool m_repaintPending;
    // used to signal whether or not the draw can use this tile.
    bool m_usable;
    // stores the id of the latest picture from webkit that caused this tile to
    // become dirty. A tile is no longer dirty when it has been painted with a
    // picture that is newer than this value.
    unsigned int m_lastDirtyPicture;

    // store the dirty region
    SkRegion m_dirtyAreaA;
    SkRegion m_dirtyAreaB;
    bool m_fullRepaintA;
    bool m_fullRepaintB;
    SkRegion* m_currentDirtyArea;

    // stores the id of the latest picture painted to the tile. If the id is 0
    // then we know that the picture has not yet been painted an there is nothing
    // to display (dirty or otherwise).
    unsigned int m_lastPaintedPicture;

    // This mutex serves two purposes. (1) It ensures that certain operations
    // happen atomically and (2) it makes sure those operations are synchronized
    // across all threads and cores.
    android::Mutex m_atomicSync;

};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // BaseTile_h
