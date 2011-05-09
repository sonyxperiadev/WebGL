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
#include "SkBitmap.h"
#include "SkBitmapRef.h"
#include "SkCanvas.h"
#include "SkPicture.h"
#include "TilesManager.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
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

BaseTile::BaseTile()
    : m_page(0)
    , m_x(-1)
    , m_y(-1)
    , m_texture(0)
    , m_scale(1)
    , m_dirty(true)
    , m_repaintPending(false)
    , m_usable(true)
    , m_lastDirtyPicture(0)
    , m_fullRepaintA(true)
    , m_fullRepaintB(true)
    , m_lastPaintedPicture(0)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("BaseTile");
#endif
    m_currentDirtyArea = &m_dirtyAreaA;
}

BaseTile::~BaseTile()
{
    setUsedLevel(-1);
    if (m_texture)
        m_texture->release(this);

#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("BaseTile");
#endif
}

// All the following functions must be called from the main GL thread.

void BaseTile::setContents(TiledPage* page, int x, int y)
{
    android::AutoMutex lock(m_atomicSync);
    m_page = page;
    m_x = x;
    m_y = y;
}

void BaseTile::reserveTexture()
{
    BackedDoubleBufferedTexture* texture = TilesManager::instance()->getAvailableTexture(this);

    android::AutoMutex lock(m_atomicSync);
    if (texture && m_texture != texture) {
        m_lastPaintedPicture = 0;
        fullInval();
        m_texture = texture;
    }
}

bool BaseTile::removeTexture(BackedDoubleBufferedTexture* texture)
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
    m_dirtyAreaA.setEmpty();
    m_dirtyAreaB.setEmpty();
    m_fullRepaintA = true;
    m_fullRepaintB = true;
    m_dirty = true;
}

void BaseTile::setScale(float scale)
{
    android::AutoMutex lock(m_atomicSync);
    if (m_scale != scale) {
        m_scale = scale;
        fullInval();
    }
}

void BaseTile::markAsDirty(int unsigned pictureCount,
                           const SkRegion& dirtyArea)
{
    android::AutoMutex lock(m_atomicSync);
    m_lastDirtyPicture = pictureCount;
    m_dirtyAreaA.op(dirtyArea, SkRegion::kUnion_Op);
    m_dirtyAreaB.op(dirtyArea, SkRegion::kUnion_Op);
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
}

int BaseTile::usedLevel()
{
    if (m_texture)
        return m_texture->usedLevel();
    return -1;
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
        XLOG("draw tile %d, %d, %.2f with texture %x", x(), y(), scale(), m_texture);
        TilesManager::instance()->shader()->drawQuad(rect, textureInfo->m_textureId,
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

void BaseTile::drawTileInfo(SkCanvas* canvas,
                            BackedDoubleBufferedTexture* texture,
                            int x, int y, float scale,
                            int pictureCount)
{
    SkPaint paint;
    char str[256];
    snprintf(str, 256, "(%d,%d) %.2f, tl%x tx%x p%x c%x",
             x, y, scale, this, texture, m_page, pictureCount);
    paint.setARGB(255, 0, 0, 0);
    canvas->drawText(str, strlen(str), 0, 10, paint);
    paint.setARGB(255, 255, 0, 0);
    canvas->drawText(str, strlen(str), 0, 11, paint);
}

// This is called from the texture generation thread
void BaseTile::paintBitmap()
{

    // We acquire the values below atomically. This ensures that we are reading
    // values correctly across cores. Further, once we have these values they
    // can be updated by other threads without consequence.
    m_atomicSync.lock();
    bool dirty = m_dirty;
    BackedDoubleBufferedTexture* texture = m_texture;
    SkRegion dirtyArea = *m_currentDirtyArea;
    float scale = m_scale;
    const int x = m_x;
    const int y = m_y;
    m_atomicSync.unlock();

    if (!dirty || !texture) {
        return;
    }

    TiledPage* tiledPage = m_page;

    texture->producerAcquireContext();
    TextureInfo* textureInfo = texture->producerLock();

    // at this point we can safely check the ownership (if the texture got
    // transferred to another BaseTile under us)
    if (texture->owner() != this || texture->usedLevel() > 1) {
        texture->producerRelease();
        return;
    }

    SkSize size = texture->getSize();
    float tileWidth = size.width();
    float tileHeight = size.height();

    const float invScale = 1 / scale;
    float w = tileWidth * invScale;
    float h = tileHeight * invScale;

    SkCanvas* canvas;
    unsigned int pictureCount = 0;

    SkRegion::Iterator cliperator(dirtyArea);

    bool fullRepaint = false;
    if (((m_currentDirtyArea == &m_dirtyAreaA) && m_fullRepaintA) ||
        ((m_currentDirtyArea == &m_dirtyAreaB) && m_fullRepaintB))
        fullRepaint = true;

    if (fullRepaint) {
        SkIRect rect;
        pictureCount = paintPartialBitmap(rect, 0, 0, scale, texture,
                           textureInfo, tiledPage, true);
    } else {
        while (!cliperator.done()) {
            SkRect dirtyRect;
            dirtyRect.set(cliperator.rect());

            float left = x * tileWidth;
            float top = y * tileHeight;

            // compute the rect to corresponds to pixels
            SkRect realTileRect;
            realTileRect.fLeft = left;
            realTileRect.fTop = top;
            realTileRect.fRight = left + tileWidth;
            realTileRect.fBottom = top + tileHeight;

            // scale the dirtyRect for intersect computation.
            SkRect realDirtyRect = SkRect::MakeWH(dirtyRect.width() * scale,
                                                  dirtyRect.height() * scale);
            realDirtyRect.offset(dirtyRect.fLeft * scale, dirtyRect.fTop * scale);

            if (!realTileRect.intersect(realDirtyRect)) {
                cliperator.next();
                continue;
            }

            realTileRect.fLeft = floorf(realTileRect.fLeft);
            realTileRect.fTop = floorf(realTileRect.fTop);
            realTileRect.fRight = ceilf(realTileRect.fRight);
            realTileRect.fBottom = ceilf(realTileRect.fBottom);

            SkIRect finalRealRect;
            finalRealRect.fLeft = static_cast<int>(realTileRect.fLeft) % static_cast<int>(tileWidth);
            finalRealRect.fTop = static_cast<int>(realTileRect.fTop) % static_cast<int>(tileHeight);
            finalRealRect.fRight = finalRealRect.fLeft + realTileRect.width();
            finalRealRect.fBottom = finalRealRect.fTop + realTileRect.height();

            // the canvas translate can be recomputed accounting for the scale
            float tx = - realTileRect.fLeft / scale;
            float ty = - realTileRect.fTop / scale;

            pictureCount = paintPartialBitmap(finalRealRect, tx, ty, scale, texture,
                                              textureInfo, tiledPage);

            cliperator.next();
        }
    }
    XLOG("%x update texture %x for tile %d, %d scale %.2f (m_scale: %.2f)", this, textureInfo, x, y, scale, m_scale);

    m_atomicSync.lock();
    texture->setTile(textureInfo, x, y, scale, pictureCount);
    texture->producerReleaseAndSwap();

    if (texture == m_texture) {
        m_lastPaintedPicture = pictureCount;

        // set the fullrepaint flags

        if ((m_currentDirtyArea == &m_dirtyAreaA) && m_fullRepaintA)
            m_fullRepaintA = false;

        if ((m_currentDirtyArea == &m_dirtyAreaB) && m_fullRepaintB)
            m_fullRepaintB = false;

        // The various checks to see if we are still dirty...

        m_dirty = false;

        if (m_scale != scale)
            m_dirty = true;

        if (!fullRepaint)
            m_currentDirtyArea->op(dirtyArea, SkRegion::kDifference_Op);

        if (!m_currentDirtyArea->isEmpty())
            m_dirty = true;

        // Now we can swap the dirty areas

        m_currentDirtyArea = m_currentDirtyArea == &m_dirtyAreaA ? &m_dirtyAreaB : &m_dirtyAreaA;

        if (!m_currentDirtyArea->isEmpty())
            m_dirty = true;

        if (!m_dirty)
            m_usable = true;
    }

    m_atomicSync.unlock();
}

int BaseTile::paintPartialBitmap(SkIRect r, float ptx, float pty,
                                  float scale, BackedDoubleBufferedTexture* texture,
                                  TextureInfo* textureInfo,
                                  TiledPage* tiledPage, bool fullRepaint)
{
    SkIRect rect = r;
    float tx = ptx;
    float ty = pty;
    if (!texture->textureExist(textureInfo)) {
        fullRepaint = true;
    }

    if ((rect.width() > TilesManager::instance()->tileWidth()) ||
        (rect.height() > TilesManager::instance()->tileHeight()))
        fullRepaint = true;

    if (fullRepaint) {
        rect.set(0, 0, TilesManager::instance()->tileWidth(),
                 TilesManager::instance()->tileHeight());
        tx = - x() * TilesManager::instance()->tileWidth() / scale;
        ty = - y() * TilesManager::instance()->tileHeight() / scale;
    }

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height());
    bitmap.allocPixels();
    bitmap.eraseColor(0);

    SkCanvas canvas(bitmap);
    canvas.drawARGB(255, 255, 255, 255);

    SkPicture picture;
    SkCanvas* nCanvas = picture.beginRecording(rect.width(), rect.height());
    nCanvas->scale(scale, scale);
    nCanvas->translate(tx, ty);
    int pictureCount = tiledPage->paintBaseLayerContent(nCanvas);
    picture.endRecording();

    bool visualIndicator = TilesManager::instance()->getShowVisualIndicator();
    if (visualIndicator)
        canvas.save();
    picture.draw(&canvas);
    if (visualIndicator)
        canvas.restore();

    if (visualIndicator) {
        int color = 20 + pictureCount % 100;
        canvas.drawARGB(color, 0, 255, 0);

        SkPaint paint;
        paint.setARGB(128, 255, 0, 0);
        paint.setStrokeWidth(3);
        canvas.drawLine(0, 0, rect.width(), rect.height(), paint);
        paint.setARGB(128, 0, 255, 0);
        canvas.drawLine(0, rect.height(), rect.width(), 0, paint);
        paint.setARGB(128, 0, 0, 255);
        canvas.drawLine(0, 0, rect.width(), 0, paint);
        canvas.drawLine(rect.width(), 0, rect.width(), rect.height(), paint);

        drawTileInfo(&canvas, texture, x(), y(), scale, pictureCount);
    }

    if (!texture->textureExist(textureInfo)) {
        GLUtils::createTextureWithBitmap(textureInfo->m_textureId, bitmap);
        textureInfo->m_width = rect.width();
        textureInfo->m_height = rect.height();
    } else {
        GLUtils::updateTextureWithBitmap(textureInfo->m_textureId, rect.fLeft, rect.fTop, bitmap);
    }

    bitmap.reset();

    return pictureCount;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
