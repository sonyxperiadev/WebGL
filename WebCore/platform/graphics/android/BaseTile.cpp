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
    , m_usable(true)
    , m_lastDirtyPicture(0)
    , m_lastPaintedPicture(0)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("BaseTile");
#endif
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
    if (m_texture != texture) {
        m_lastPaintedPicture = 0;
        m_dirty = true;
    }
    m_texture = texture;
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

void BaseTile::setScale(float scale)
{
    android::AutoMutex lock(m_atomicSync);
    if (m_scale != scale)
        m_dirty = true;
    m_scale = scale;
}

void BaseTile::markAsDirty(int unsigned pictureCount)
{
    android::AutoMutex lock(m_atomicSync);
    m_lastDirtyPicture = pictureCount;
    if (m_lastPaintedPicture < m_lastDirtyPicture)
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

void BaseTile::setUsedLevel(int usedLevel)
{
    if (m_texture)
        m_texture->setUsedLevel(usedLevel);
}

void BaseTile::draw(float transparency, SkRect& rect)
{
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

    if (m_texture->x() != m_x || m_texture->y() != m_y)
        return;

    TextureInfo* textureInfo = m_texture->consumerLock();
    if (!textureInfo) {
        XLOG("%x (%d, %d) trying to draw, but no textureInfo!", this, x(), y());
        m_texture->consumerRelease();
        return;
    }

    TilesManager::instance()->shader()->drawQuad(rect, textureInfo->m_textureId,
                                                 transparency);

    m_texture->consumerRelease();
}

bool BaseTile::isTileReady()
{
    if (!m_texture)
        return false;
    if (m_texture->owner() != this)
        return false;

    android::AutoMutex lock(m_atomicSync);
    return !m_dirty;
}

void BaseTile::drawTileInfo(SkCanvas* canvas,
                            BackedDoubleBufferedTexture* texture,
                            int x, int y, float scale)
{
    SkPaint paint;
    char str[256];
    snprintf(str, 256, "(%d,%d) %.2f, tile %x, texture: %x",
             x, y, scale, this, texture);
    paint.setARGB(255, 0, 0, 0);
    canvas->drawText(str, strlen(str), 50, 100, paint);
    paint.setARGB(255, 255, 0, 0);
    canvas->drawText(str, strlen(str), 51, 101, paint);
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
    float scale = m_scale;
    m_atomicSync.unlock();

    if (!dirty || !texture)
        return;

    const int x = m_x;
    const int y = m_y;
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

    SkCanvas* canvas = texture->canvas();

    canvas->save();
    canvas->drawColor(tiledPage->glWebViewState()->getBackgroundColor());
    canvas->scale(scale, scale);
    canvas->translate(-x * w, -y * h);

    unsigned int pictureCount = tiledPage->paintBaseLayerContent(canvas);

    canvas->restore();

    if (TilesManager::instance()->getShowVisualIndicator()) {
        SkPaint paint;
        paint.setARGB(128, 255, 0, 0);
        paint.setStrokeWidth(3);
        canvas->drawLine(0, 0, tileWidth, tileHeight, paint);
        paint.setARGB(128, 0, 255, 0);
        canvas->drawLine(0, tileHeight, tileWidth, 0, paint);
        paint.setARGB(128, 0, 0, 255);
        canvas->drawLine(0, 0, tileWidth, 0, paint);
        canvas->drawLine(tileWidth, 0, tileWidth, tileHeight, paint);
        drawTileInfo(canvas, texture, x, y, scale);
    }

    texture->setTile(x, y);
    texture->producerUpdate(textureInfo);

    m_atomicSync.lock();
    m_lastPaintedPicture = pictureCount;
    if (m_lastPaintedPicture >= m_lastDirtyPicture) {
        m_dirty = false;
        m_usable = true;
    }
    m_atomicSync.unlock();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
