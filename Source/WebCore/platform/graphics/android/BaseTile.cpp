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
#include "BaseTile.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GLUtils.h"
#include "RasterRenderer.h"
#include "TextureInfo.h"
#include "TilesManager.h"

#include <cutils/atomic.h>

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "BaseTile", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "BaseTile", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

BaseTile::BaseTile(bool isLayerTile)
    : m_glWebViewState(0)
    , m_painter(0)
    , m_x(-1)
    , m_y(-1)
    , m_page(0)
    , m_frontTexture(0)
    , m_backTexture(0)
    , m_scale(1)
    , m_dirty(true)
    , m_repaintPending(false)
    , m_lastDirtyPicture(0)
    , m_isTexturePainted(false)
    , m_isLayerTile(isLayerTile)
    , m_drawCount(0)
    , m_state(Unpainted)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("BaseTile");
#endif
    m_currentDirtyAreaIndex = 0;

    // For EglImage Mode, the internal buffer should be 2.
    // For Surface Texture mode, we only need one.
    if (TilesManager::instance()->getSharedTextureMode() == EglImageMode)
        m_maxBufferNumber = 2;
    else
        m_maxBufferNumber = 1;

    m_dirtyArea = new SkRegion[m_maxBufferNumber];
    m_fullRepaint = new bool[m_maxBufferNumber];
    for (int i = 0; i < m_maxBufferNumber; i++)
        m_fullRepaint[i] = true;

    m_renderer = BaseRenderer::createRenderer();
}

BaseTile::~BaseTile()
{
    if (m_backTexture)
        m_backTexture->release(this);
    if (m_frontTexture)
        m_frontTexture->release(this);

    delete m_renderer;
    delete[] m_dirtyArea;
    delete[] m_fullRepaint;

#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("BaseTile");
#endif
}

// All the following functions must be called from the main GL thread.

void BaseTile::setContents(TilePainter* painter, int x, int y, float scale)
{
    if ((m_painter != painter)
        || (m_x != x)
        || (m_y != y)
        || (m_scale != scale)) {
        // neither texture is relevant
        discardTextures();
    }

    android::AutoMutex lock(m_atomicSync);
    m_painter = painter;
    m_x = x;
    m_y = y;
    m_scale = scale;
    m_drawCount = TilesManager::instance()->getDrawGLCount();
}

void BaseTile::reserveTexture()
{
    BaseTileTexture* texture = TilesManager::instance()->getAvailableTexture(this);

    android::AutoMutex lock(m_atomicSync);
    if (texture && m_backTexture != texture) {
        XLOG("tile %p reserving texture %p, back was %p (front %p)",
             this, texture, m_backTexture, m_frontTexture);
        m_state = Unpainted;
        m_backTexture = texture;
    }

    if (m_state == UpToDate) {
        XLOG("moving tile %p to unpainted, since it reserved while up to date", this);
        m_dirty = true;
        m_state = Unpainted;
    }
}

bool BaseTile::removeTexture(BaseTileTexture* texture)
{
    XLOG("%p removeTexture %p, back %p front %p... page %p",
         this, texture, m_backTexture, m_frontTexture, m_page);
    // We update atomically, so paintBitmap() can see the correct value
    android::AutoMutex lock(m_atomicSync);
    if (m_frontTexture == texture) {
        if (m_state == UpToDate) {
            XLOG("front texture removed, state was UpToDate, now becoming unpainted, bt is %p", m_backTexture);
            m_state = Unpainted;
        }

        m_frontTexture = 0;
    }
    if (m_backTexture == texture) {
        m_state = Unpainted;
        m_backTexture = 0;
    }

    // mark dirty regardless of which texture was taken - the back texture may
    // have been ready to swap
    m_dirty = true;

    return true;
}

void BaseTile::markAsDirty(int unsigned pictureCount,
                           const SkRegion& dirtyArea)
{
    if (dirtyArea.isEmpty())
        return;
    android::AutoMutex lock(m_atomicSync);
    m_lastDirtyPicture = pictureCount;
    for (int i = 0; i < m_maxBufferNumber; i++)
        m_dirtyArea[i].op(dirtyArea, SkRegion::kUnion_Op);

    // Check if we actually intersect with the area
    bool intersect = false;
    SkRegion::Iterator cliperator(dirtyArea);
    int tileWidth = TilesManager::instance()->tileWidth();
    int tileHeight = TilesManager::instance()->tileHeight();
    if (m_isLayerTile) {
        tileWidth = TilesManager::instance()->layerTileWidth();
        tileHeight = TilesManager::instance()->layerTileHeight();
    }
    SkRect realTileRect;
    SkRect dirtyRect;
    while (!cliperator.done()) {
        dirtyRect.set(cliperator.rect());
        if (intersectWithRect(m_x, m_y, tileWidth, tileHeight,
                              m_scale, dirtyRect, realTileRect)) {
            intersect = true;
            break;
        }
        cliperator.next();
    }

    if (!intersect)
        return;

    m_dirty = true;
    if (m_state == UpToDate) {
        // We only mark a tile as unpainted in 'markAsDirty' if its status is
        // UpToDate: marking dirty means we need to repaint, but don't stop the
        // current paint
        m_state = Unpainted;
    } else if (m_state != Unpainted) {
        // TODO: fix it so that they can paint while deferring the markAsDirty
        // call (or block updates)
        XLOG("Warning: tried to mark tile %p at %d, %d islayertile %d as dirty, state %d, page %p",
              this, m_x, m_y, isLayerTile(), m_state, m_page);

        // prefetch tiles can be marked dirty while in the process of painting,
        // due to not using an update lock. force them to fail validate step.
        m_state = Unpainted;
    }
}

bool BaseTile::isDirty()
{
    android::AutoMutex lock(m_atomicSync);
    return m_dirty;
}

bool BaseTile::isRepaintPending()
{
    android::AutoMutex lock(m_atomicSync);
    return m_repaintPending;
}

void BaseTile::setRepaintPending(bool pending)
{
    android::AutoMutex lock(m_atomicSync);
    m_repaintPending = pending;
}

void BaseTile::draw(float transparency, SkRect& rect, float scale)
{
    if (m_x < 0 || m_y < 0 || m_scale != scale)
        return;

    // No need to mutex protect reads of m_backTexture as it is only written to by
    // the consumer thread.
    if (!m_frontTexture)
        return;

    // Early return if set to un-usable in purpose!
    m_atomicSync.lock();
    bool isTexturePainted = m_isTexturePainted;
    m_atomicSync.unlock();

    if (!isTexturePainted)
        return;

    TextureInfo* textureInfo = m_frontTexture->consumerLock();
    if (!textureInfo) {
        m_frontTexture->consumerRelease();
        return;
    }

    if (m_frontTexture->readyFor(this)) {
        if (isLayerTile() && m_painter && m_painter->transform())
            TilesManager::instance()->shader()->drawLayerQuad(*m_painter->transform(),
                                                              rect, m_frontTexture->m_ownTextureId,
                                                              transparency, true);
        else
            TilesManager::instance()->shader()->drawQuad(rect, m_frontTexture->m_ownTextureId,
                                                         transparency);
    } else {
        XLOG("tile %p at %d, %d not readyfor (at draw),", this, m_x, m_y);
    }

    m_frontTexture->consumerRelease();
}

bool BaseTile::isTileReady()
{
    // Return true if the tile's most recently drawn texture is up to date
    android::AutoMutex lock(m_atomicSync);
    BaseTileTexture * texture = (m_state == ReadyToSwap) ? m_backTexture : m_frontTexture;

    if (!texture)
        return false;

    if (texture->owner() != this)
        return false;

    if (m_dirty)
        return false;

    if (m_state != ReadyToSwap && m_state != UpToDate)
        return false;

    texture->consumerLock();
    bool ready = texture->readyFor(this);
    texture->consumerRelease();

    if (ready)
        return true;

    XLOG("tile %p at %d, %d not readyfor (at isTileReady)", this, m_x, m_y);

    return false;
}

bool BaseTile::intersectWithRect(int x, int y, int tileWidth, int tileHeight,
                                 float scale, const SkRect& dirtyRect,
                                 SkRect& realTileRect)
{
    // compute the rect to corresponds to pixels
    realTileRect.fLeft = x * tileWidth;
    realTileRect.fTop = y * tileHeight;
    realTileRect.fRight = realTileRect.fLeft + tileWidth;
    realTileRect.fBottom = realTileRect.fTop + tileHeight;

    // scale the dirtyRect for intersect computation.
    SkRect realDirtyRect = SkRect::MakeWH(dirtyRect.width() * scale,
                                          dirtyRect.height() * scale);
    realDirtyRect.offset(dirtyRect.fLeft * scale, dirtyRect.fTop * scale);

    if (!realTileRect.intersect(realDirtyRect))
        return false;
    return true;
}

bool BaseTile::isTileVisible(const IntRect& viewTileBounds)
{
    return (m_x >= viewTileBounds.x()
            && m_x < viewTileBounds.x() + viewTileBounds.width()
            && m_y >= viewTileBounds.y()
            && m_y < viewTileBounds.y() + viewTileBounds.height());
}

// This is called from the texture generation thread
void BaseTile::paintBitmap()
{
    // We acquire the values below atomically. This ensures that we are reading
    // values correctly across cores. Further, once we have these values they
    // can be updated by other threads without consequence.
    m_atomicSync.lock();
    bool dirty = m_dirty;
    BaseTileTexture* texture = m_backTexture;
    SkRegion dirtyArea = m_dirtyArea[m_currentDirtyAreaIndex];
    float scale = m_scale;
    const int x = m_x;
    const int y = m_y;
    TilePainter* painter = m_painter;

    if (!dirty || !texture) {
        m_atomicSync.unlock();
        return;
    }
    if (m_state != Unpainted) {
        XLOG("Warning: started painting tile %p, but was at state %d, ft %p bt %p",
              this, m_state, m_frontTexture, m_backTexture);
    }
    m_state = PaintingStarted;

    texture->producerAcquireContext();
    TextureInfo* textureInfo = texture->producerLock();
    m_atomicSync.unlock();

    // at this point we can safely check the ownership (if the texture got
    // transferred to another BaseTile under us)
    if (texture->owner() != this) {
        texture->producerRelease();
        return;
    }

    unsigned int pictureCount = 0;

    // swap out the renderer if necessary
    BaseRenderer::swapRendererIfNeeded(m_renderer);

    // setup the common renderInfo fields;
    TileRenderInfo renderInfo;
    renderInfo.x = x;
    renderInfo.y = y;
    renderInfo.scale = scale;
    renderInfo.tileSize = texture->getSize();
    renderInfo.tilePainter = painter;
    renderInfo.baseTile = this;
    renderInfo.textureInfo = textureInfo;

    const float tileWidth = renderInfo.tileSize.width();
    const float tileHeight = renderInfo.tileSize.height();

    SkRegion::Iterator cliperator(dirtyArea);

    bool fullRepaint = false;

    if (m_fullRepaint[m_currentDirtyAreaIndex]
        || textureInfo->m_width != tileWidth
        || textureInfo->m_height != tileHeight) {
        fullRepaint = true;
    }

    bool surfaceTextureMode = textureInfo->getSharedTextureMode() == SurfaceTextureMode;

    if (surfaceTextureMode)
        fullRepaint = true;

    while (!fullRepaint && !cliperator.done()) {
        SkRect realTileRect;
        SkRect dirtyRect;
        dirtyRect.set(cliperator.rect());
        bool intersect = intersectWithRect(x, y, tileWidth, tileHeight,
                                           scale, dirtyRect, realTileRect);

        // With SurfaceTexture, just repaint the entire tile if we intersect
        // TODO: Implement the partial invalidate in Surface Texture Mode
        if (intersect && surfaceTextureMode) {
            fullRepaint = true;
            break;
        }

        if (intersect && !surfaceTextureMode) {
            // initialize finalRealRect to the rounded values of realTileRect
            SkIRect finalRealRect;
            realTileRect.roundOut(&finalRealRect);

            // stash the int values of the current width and height
            const int iWidth = finalRealRect.width();
            const int iHeight = finalRealRect.height();

            if (iWidth == tileWidth || iHeight == tileHeight) {
                fullRepaint = true;
                break;
            }

            // translate the rect into tile space coordinates
            finalRealRect.fLeft = finalRealRect.fLeft % static_cast<int>(tileWidth);
            finalRealRect.fTop = finalRealRect.fTop % static_cast<int>(tileHeight);
            finalRealRect.fRight = finalRealRect.fLeft + iWidth;
            finalRealRect.fBottom = finalRealRect.fTop + iHeight;

            renderInfo.invalRect = &finalRealRect;
            renderInfo.measurePerf = false;

            pictureCount = m_renderer->renderTiledContent(renderInfo);
        }

        cliperator.next();
    }

    // Do a full repaint if needed
    if (fullRepaint) {
        SkIRect rect;
        rect.set(0, 0, tileWidth, tileHeight);

        renderInfo.invalRect = &rect;
        renderInfo.measurePerf = TilesManager::instance()->getShowVisualIndicator();

        pictureCount = m_renderer->renderTiledContent(renderInfo);
    }

    m_atomicSync.lock();

#if DEPRECATED_SURFACE_TEXTURE_MODE
    texture->setTile(textureInfo, x, y, scale, painter, pictureCount);
#endif
    texture->producerReleaseAndSwap();
    if (texture == m_backTexture) {
        m_isTexturePainted = true;

        // set the fullrepaint flags
        m_fullRepaint[m_currentDirtyAreaIndex] = false;

        // The various checks to see if we are still dirty...

        m_dirty = false;

        if (m_scale != scale)
            m_dirty = true;

        if (fullRepaint)
            m_dirtyArea[m_currentDirtyAreaIndex].setEmpty();
        else
            m_dirtyArea[m_currentDirtyAreaIndex].op(dirtyArea, SkRegion::kDifference_Op);

        if (!m_dirtyArea[m_currentDirtyAreaIndex].isEmpty())
            m_dirty = true;

        // Now we can swap the dirty areas
        // TODO: For surface texture in Async mode, the index will be updated
        // according to the current buffer just dequeued.
        m_currentDirtyAreaIndex = (m_currentDirtyAreaIndex+1) % m_maxBufferNumber;

        if (!m_dirtyArea[m_currentDirtyAreaIndex].isEmpty())
            m_dirty = true;

        XLOG("painted tile %p (%d, %d), texture %p, dirty=%d", this, x, y, texture, m_dirty);

        validatePaint();
    } else {
        XLOG("tile %p no longer owns texture %p, m_state %d. ft %p bt %p",
             this, texture, m_state, m_frontTexture, m_backTexture);
    }

    m_atomicSync.unlock();
}

void BaseTile::discardTextures() {
    android::AutoMutex lock(m_atomicSync);
    XLOG("%p discarding bt %p, ft %p",
         this, m_backTexture, m_frontTexture);
    if (m_frontTexture) {
        m_frontTexture->release(this);
        m_frontTexture = 0;
    }
    if (m_backTexture) {
        m_backTexture->release(this);
        m_backTexture = 0;
    }
    for (int i = 0; i < m_maxBufferNumber; i++) {
        m_dirtyArea[i].setEmpty();
        m_fullRepaint[i] = true;
    }
    m_dirty = true;
    m_state = Unpainted;
}

void BaseTile::discardBackTexture() {
    android::AutoMutex lock(m_atomicSync);
    if (m_backTexture) {
        m_backTexture->release(this);
        m_backTexture = 0;
    }
    m_state = Unpainted;
    m_dirty = true;
}

bool BaseTile::swapTexturesIfNeeded() {
    android::AutoMutex lock(m_atomicSync);
    if (m_state == ReadyToSwap) {
        // discard old texture and swap the new one in its place
        if (m_frontTexture)
            m_frontTexture->release(this);

        m_frontTexture = m_backTexture;
        m_backTexture = 0;
        m_state = UpToDate;
        XLOG("display texture for %p at %d, %d front is now %p, back is %p",
             this, m_x, m_y, m_frontTexture, m_backTexture);

        return true;
    }
    return false;
}

void BaseTile::backTextureTransfer() {
    android::AutoMutex lock(m_atomicSync);
    if (m_state == PaintingStarted)
        m_state = TransferredUnvalidated;
    else if (m_state == ValidatedUntransferred)
        m_state = ReadyToSwap;
    else {
        // shouldn't have transferred a tile in any other state, log
        XLOG("Note: transferred tile %p at %d %d, state wasn't paintingstarted or validated: %d",
             this, m_x, m_y, m_state);
    }
}

void BaseTile::backTextureTransferFail() {
    // transfer failed for some reason, mark dirty so it will (repaint and) be
    // retransferred.
    android::AutoMutex lock(m_atomicSync);
    m_state = Unpainted;
    m_dirty = true;
    // whether validatePaint is called before or after, it won't do anything
}

void BaseTile::validatePaint() {
    // ONLY CALL while m_atomicSync is locked (at the end of paintBitmap())

    if (!m_dirty) {
        // since after the paint, the tile isn't dirty, 'validate' it - this
        // may happed before or after the transfer queue operation. Only
        // when both have happened, mark as 'ReadyToSwap'
        if (m_state == PaintingStarted)
            m_state = ValidatedUntransferred;
        else if (m_state == TransferredUnvalidated)
            m_state = ReadyToSwap;
        else {
            XLOG("Note: validated tile %p at %d %d, state wasn't paintingstarted or transferred %d",
                  this, m_x, m_y, m_state);
            // failed transferring, in which case mark dirty (since
            // paintBitmap() may have cleared m_dirty)
            m_dirty = true;
        }

        if (m_deferredDirty) {
            XLOG("Note: deferred dirty flag set, possibly a missed paint on tile %p", this);
            m_deferredDirty = false;
        }
    } else {
        XLOG("Note: paint was unsuccessful.");
        m_state = Unpainted;
    }

}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
