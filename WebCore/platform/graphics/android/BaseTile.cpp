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
static int gBaseTextureCount = 0;
int BaseTile::count()
{
    return gBaseTextureCount;
}
#endif

BaseTile::BaseTile(TiledPage* page, int x, int y, int quality)
    : m_page(page)
    , m_texture(0)
    , m_x(x)
    , m_y(y)
    , m_quality(quality)
    , m_scale(1)
{
#ifdef DEBUG_COUNT
    gBaseTextureCount++;
#endif
}

BaseTile::~BaseTile()
{
    removeTexture();
    setUsedLevel(-1);
#ifdef DEBUG_COUNT
    gBaseTextureCount--;
#endif
}

void BaseTile::reserveTexture()
{
    android::Mutex::Autolock lock(m_varLock);
    m_texture = TilesManager::instance()->getAvailableTexture(this);
    XLOG("%x (%d, %d) reserveTexture res: %x...", this, x(), y(), m_texture);
}

void BaseTile::removeTexture()
{
    android::Mutex::Autolock lock(m_varLock);
    XLOG("%x removeTexture res: %x...", this, m_texture);
    m_texture = 0;
}

void BaseTile::setUsedLevel(int usedLevel)
{
    if (m_texture)
        m_texture->setUsedLevel(usedLevel);
}

// Called from the main GL thread
void BaseTile::draw(float transparency, SkRect& rect)
{
    m_varLock.lock();
    BackedDoubleBufferedTexture* texture = m_texture;
    m_varLock.unlock();

    if (!m_texture) {
        XLOG("%x (%d, %d) trying to draw, but no m_texture!", this, x(), y());
        return;
    }

    PaintingInfo info(m_x, m_y, m_page->glWebViewState(),
                      m_page->currentPictureCounter());

    TextureInfo* textureInfo = texture->consumerLock();
    if (!textureInfo) {
        XLOG("%x (%d, %d) trying to draw, but no textureInfo!", this, x(), y());
        texture->consumerRelease();
        return;
    }

    if (texture->consumerTextureSimilar(info)) {
        TilesManager::instance()->shader()->drawQuad(rect, textureInfo->m_textureId,
                                                     transparency);
    }

    texture->consumerRelease();
}

bool BaseTile::isBitmapReady()
{
    if (!m_texture)
        return false;
    if (m_texture->owner() != this)
        return false;
    PaintingInfo info(m_x, m_y, m_page->glWebViewState(),
                      m_page->currentPictureCounter());
    return m_texture->consumerTextureUpToDate(info);
}

// Called from the texture generation thread
bool BaseTile::paintBitmap()
{
    m_varLock.lock();
    BackedDoubleBufferedTexture* texture = m_texture;
    bool available = false;
    if (texture)
        available = texture->acquireForPainting();
    m_varLock.unlock();

    if (!texture || !available)
        return false;

    // at this point we can safely check the ownership
    // (if the texture got transferred to another BaseTile
    // under us)
    if (texture->owner() != this
        || texture->usedLevel() > 1) {
        texture->setBusy(false);
        return false;
    }

    int pictureUsed = m_page->currentPictureCounter();
    PaintingInfo info(m_x, m_y, m_page->glWebViewState(), pictureUsed);
    if (texture->consumerTextureUpToDate(info)) {
        texture->setBusy(false);
        return true;
    }

    TextureInfo* textureInfo = texture->producerLock();

    if (!textureInfo) {
        texture->setBusy(false);
        return false;
    }

    float tileWidth = textureInfo->m_width;
    float tileHeight = textureInfo->m_height;

    float scale = m_scale / m_quality;
    float invScale = 1 / scale;

    float w = tileWidth * invScale;
    float h = tileHeight * invScale;

    SkCanvas* canvas = texture->canvas();

    canvas->save();

    canvas->scale(scale, scale);
    canvas->translate(-m_x*w, -m_y*h);

    bool didPaint = m_page->paintBaseLayerContent(canvas);

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

    texture->update(textureInfo, info);
    texture->setPainter(this);
    texture->setBusy(false);
    texture->producerRelease();

    return didPaint;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
