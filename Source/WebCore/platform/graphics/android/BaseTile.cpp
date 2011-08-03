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

#ifdef DEBUG

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

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
    , m_usedLevel(-1)
    , m_texture(0)
    , m_scale(1)
    , m_dirty(true)
    , m_repaintPending(false)
    , m_usable(true)
    , m_lastDirtyPicture(0)
    , m_lastPaintedPicture(0)
    , m_isLayerTile(isLayerTile)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("BaseTile");
#endif
    m_currentDirtyAreaIndex = 0;
    m_renderer = new RasterRenderer();

    // For EglImage Mode, the internal buffer should be 2.
    // And for Async Surface Texture mode, this is 3.
    if (TilesManager::instance()->getSharedTextureMode() == EglImageMode)
        m_maxBufferNumber = 2;
    else
        m_maxBufferNumber = 3;

    m_dirtyArea = new SkRegion[m_maxBufferNumber];
    m_fullRepaint = new bool[m_maxBufferNumber];
    for (int i = 0; i < m_maxBufferNumber; i++)
        m_fullRepaint[i] = true;

    m_renderer = BaseRenderer::createRenderer();
}

BaseTile::~BaseTile()
{
    setUsedLevel(-1);
    if (m_texture)
        m_texture->release(this);

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
    android::AutoMutex lock(m_atomicSync);
    if ((m_painter != painter)
        || (m_x != x)
        || (m_y != y)
        || (m_scale != scale))
        fullInval();

    m_painter = painter;
    m_x = x;
    m_y = y;
    m_scale = scale;
}

void BaseTile::reserveTexture()
{
    BaseTileTexture* texture = TilesManager::instance()->getAvailableTexture(this);

    android::AutoMutex lock(m_atomicSync);
    if (texture && m_texture != texture) {
        m_lastPaintedPicture = 0;
        fullInval();
    }
    m_texture = texture;
    if (m_texture)
        m_texture->setUsedLevel(m_usedLevel);
}

bool BaseTile::removeTexture(BaseTileTexture* texture)
{
    XLOG("%x removeTexture res: %x... page %x", this, m_texture, m_page);
    // We update atomically, so paintBitmap() can see the correct value
    android::AutoMutex lock(m_atomicSync);
    if (m_texture == texture)
        m_texture = 0;
    return true;
}

void BaseTile::fullInval()
{
    for (int i = 0; i < m_maxBufferNumber; i++) {
        m_dirtyArea[i].setEmpty();
        m_fullRepaint[i] = true;
    }
    m_dirty = true;
}

void BaseTile::markAsDirty(int unsigned pictureCount,
                           const SkRegion& dirtyArea)
{
    android::AutoMutex lock(m_atomicSync);
    m_lastDirtyPicture = pictureCount;
    for (int i = 0; i < m_maxBufferNumber; i++)
        m_dirtyArea[i].op(dirtyArea, SkRegion::kUnion_Op);
    m_dirty = true;
}

void BaseTile::setUsable(bool usable)
{
    android::AutoMutex lock(m_atomicSync);
    m_usable = usable;
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

void BaseTile::setUsedLevel(int usedLevel)
{
    if (m_texture)
        m_texture->setUsedLevel(usedLevel);
    m_usedLevel = usedLevel;
}

int BaseTile::usedLevel()
{
    if (m_texture)
        return m_texture->usedLevel();
    return m_usedLevel;
}


void BaseTile::draw(float transparency, SkRect& rect, float scale)
{
    if (m_x < 0 || m_y < 0 || m_scale != scale)
        return;

    // No need to mutex protect reads of m_texture as it is only written to by
    // the consumer thread.
    if (!m_texture) {
        XLOG("%x on page %x (%d, %d) trying to draw, but no m_texture!", this, m_page, x(), y());
        return;
    }

    // Early return if set to un-usable in purpose!
    m_atomicSync.lock();
    bool usable = m_usable;
    bool isTexturePainted = m_lastPaintedPicture;
    m_atomicSync.unlock();
    if (!usable) {
        XLOG("early return at BaseTile::draw b/c tile set to unusable !");
        return;
    }
    if (!isTexturePainted) {
        XLOG("early return at BaseTile::draw b/c tile is not painted !");
        return;
    }

    TextureInfo* textureInfo = m_texture->consumerLock();
    if (!textureInfo) {
        XLOG("%x (%d, %d) trying to draw, but no textureInfo!", this, x(), y());
        m_texture->consumerRelease();
        return;
    }

    if (m_texture->readyFor(this)) {
        XLOG("draw tile %x : %d, %d, %.2f with texture %x", this, x(), y(), m_scale, m_texture);
        if (isLayerTile())
            TilesManager::instance()->shader()->drawLayerQuad(*m_painter->transform(),
                                                              rect, m_texture->m_ownTextureId,
                                                              transparency, true);
        else
            TilesManager::instance()->shader()->drawQuad(rect, m_texture->m_ownTextureId,
                                                         transparency);
    }
    m_texture->consumerRelease();
}

bool BaseTile::isTileReady()
{
    if (!m_texture)
        return false;
    if (m_texture->owner() != this)
        return false;

    android::AutoMutex lock(m_atomicSync);
    if (m_dirty)
        return false;

    m_texture->consumerLock();
    bool ready = m_texture->readyFor(this);
    m_texture->consumerRelease();

    if (ready)
        return true;

    m_dirty = true;
    return false;
}

// This is called from the texture generation thread
void BaseTile::paintBitmap()
{
    // We acquire the values below atomically. This ensures that we are reading
    // values correctly across cores. Further, once we have these values they
    // can be updated by other threads without consequence.
    m_atomicSync.lock();
    bool dirty = m_dirty;
    BaseTileTexture* texture = m_texture;
    SkRegion dirtyArea = m_dirtyArea[m_currentDirtyAreaIndex];
    float scale = m_scale;
    const int x = m_x;
    const int y = m_y;
    TilePainter* painter = m_painter;

    if (!dirty || !texture) {
        m_atomicSync.unlock();
        return;
    }

    texture->producerAcquireContext();
    TextureInfo* textureInfo = texture->producerLock();
    m_atomicSync.unlock();

    // at this point we can safely check the ownership (if the texture got
    // transferred to another BaseTile under us)
    if (texture->owner() != this || texture->usedLevel() > 1) {
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

    // TODO: Implement the partial invalidate in Surface Texture Mode
    if (m_fullRepaint[m_currentDirtyAreaIndex]
        || textureInfo->m_width != tileWidth
        || textureInfo->m_height != tileHeight
        || textureInfo->getSharedTextureMode() == SurfaceTextureMode) {
        fullRepaint = true;
    }

    if (!fullRepaint) {
        while (!cliperator.done()) {
            SkRect dirtyRect;
            dirtyRect.set(cliperator.rect());

            // compute the rect to corresponds to pixels
            SkRect realTileRect;
            realTileRect.fLeft = x * tileWidth;
            realTileRect.fTop = y * tileHeight;
            realTileRect.fRight = realTileRect.fLeft + tileWidth;
            realTileRect.fBottom = realTileRect.fTop + tileHeight;

            // scale the dirtyRect for intersect computation.
            SkRect realDirtyRect = SkRect::MakeWH(dirtyRect.width() * scale,
                                                  dirtyRect.height() * scale);
            realDirtyRect.offset(dirtyRect.fLeft * scale, dirtyRect.fTop * scale);

            // set realTileRect to the intersection of itself and the dirty rect
            if (!realTileRect.intersect(realDirtyRect)) {
                cliperator.next();
                continue;
            }

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

            cliperator.next();
        }
    }

    if (fullRepaint) {
        SkIRect rect;
        rect.set(0, 0, tileWidth, tileHeight);

        renderInfo.invalRect = &rect;
        renderInfo.measurePerf = TilesManager::instance()->getShowVisualIndicator();

        pictureCount = m_renderer->renderTiledContent(renderInfo);
    }

    XLOG("%x update texture %x for tile %d, %d scale %.2f (m_scale: %.2f)", this, textureInfo, x, y, scale, m_scale);

    m_atomicSync.lock();

#if DEPRECATED_SURFACE_TEXTURE_MODE
    texture->setTile(textureInfo, x, y, scale, painter, pictureCount);
#endif
    texture->producerReleaseAndSwap();

    if (texture == m_texture) {
        m_lastPaintedPicture = pictureCount;

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

        if (!m_dirty)
            m_usable = true;
    }

    m_atomicSync.unlock();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
