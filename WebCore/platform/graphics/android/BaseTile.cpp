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

#ifdef DEBUG_COUNT
static int gBaseTileCount = 0;
int BaseTile::count()
{
    return gBaseTileCount;
}
#endif

BaseTile::BaseTile(TiledPage* page, int x, int y)
    : m_page(page)
    , m_x(x)
    , m_y(y)
    , m_texture(0)
    , m_scale(1)
{
#ifdef DEBUG_COUNT
    gBaseTileCount++;
#endif
}

BaseTile::~BaseTile()
{
#ifdef DEBUG_COUNT
    gBaseTileCount--;
#endif
}

// All the following functions must be called from the main GL thread.

void BaseTile::reserveTexture()
{
    BackedDoubleBufferedTexture* texture = TilesManager::instance()->getAvailableTexture(this);
    // We update atomically, so paintBitmap() can see the correct value
    android_atomic_acquire_store((int32_t)texture, (int32_t*)&m_texture);
    XLOG("%x (%d, %d) reserveTexture res: %x...", this, x(), y(), m_texture);
}

void BaseTile::removeTexture()
{
    XLOG("%x removeTexture res: %x...", this, m_texture);
    // We update atomically, so paintBitmap() can see the correct value
    android_atomic_acquire_store(0, (int32_t*)&m_texture);
}

void BaseTile::setScale(float scale)
{
    m_scale = scale;
    // FIXME: the following two lines force a memory barrier which causes
    // m_scale to be observable on other cores. We should replace this
    // with a dedicated system function if/when available.
    int32_t tempValue = 0;
    android_atomic_acquire_load(&tempValue);
}

void BaseTile::setUsedLevel(int usedLevel)
{
    if (m_texture)
        m_texture->setUsedLevel(usedLevel);
}

void BaseTile::draw(float transparency, SkRect& rect)
{
    if (!m_texture) {
        XLOG("%x (%d, %d) trying to draw, but no m_texture!", this, x(), y());
        return;
    }

    PaintingInfo info(m_x, m_y, m_page->glWebViewState());

    TextureInfo* textureInfo = m_texture->consumerLock();
    if (!textureInfo) {
        XLOG("%x (%d, %d) trying to draw, but no textureInfo!", this, x(), y());
        m_texture->consumerRelease();
        return;
    }

    if (m_texture->consumerTextureSimilar(info)) {
        TilesManager::instance()->shader()->drawQuad(rect, textureInfo->m_textureId,
                                                     transparency);
    }

    m_texture->consumerRelease();
}

bool BaseTile::isBitmapReady()
{
    if (!m_texture)
        return false;
    if (m_texture->owner() != this)
        return false;
    PaintingInfo info(m_x, m_y, m_page->glWebViewState());
    return m_texture->consumerTextureUpToDate(info);
}

// This is called from the texture generation thread
void BaseTile::paintBitmap()
{
    const int x = m_x;
    const int y = m_y;
    TiledPage* tiledPage = m_page;

    // We acquire the texture atomically. Once we have it, we
    // can continue with it, and m_texture can be updated without
    // consequences.
    BackedDoubleBufferedTexture* texture = reinterpret_cast<BackedDoubleBufferedTexture*>(
        android_atomic_release_load((int32_t*)&m_texture));

    // The loading of m_texture forces the execution of a memory barrier,
    // which ensures that we are observing the most recent value of m_scale
    // written by another core.
    float scale = m_scale;

    if (!texture)
        return;

    TextureInfo* textureInfo = texture->producerLock();

    // at this point we can safely check the ownership
    // (if the texture got transferred to another BaseTile
    // under us)
    if (texture->owner() != this || texture->usedLevel() > 1) {
        texture->producerRelease();
        return;
    }

    PaintingInfo info(x, y, tiledPage->glWebViewState());
    if (texture->consumerTextureUpToDate(info)) {
        texture->producerRelease();
        return;
    }

    float tileWidth = textureInfo->m_width;
    float tileHeight = textureInfo->m_height;

    const float invScale = 1 / scale;
    float w = tileWidth * invScale;
    float h = tileHeight * invScale;

    SkCanvas* canvas = texture->canvas();

    canvas->save();

    canvas->scale(scale, scale);
    canvas->translate(-x * w, -y * h);

    tiledPage->paintBaseLayerContent(canvas);

    canvas->restore();

#ifdef DEBUG
    SkPaint paint;
    paint.setARGB(128, 255, 0, 0);
    paint.setStrokeWidth(3);
    canvas->drawLine(0, 0, tileWidth, tileHeight, paint);
    paint.setARGB(128, 0, 255, 0);
    canvas->drawLine(0, tileHeight, tileWidth, 0, paint);
    paint.setARGB(128, 0, 0, 255);
    canvas->drawLine(0, 0, tileWidth, 0, paint);
    canvas->drawLine(tileWidth, 0, tileWidth, tileHeight, paint);
#endif

    texture->producerUpdate(this, textureInfo, info);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
