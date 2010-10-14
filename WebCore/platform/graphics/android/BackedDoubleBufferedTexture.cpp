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
#include "BackedDoubleBufferedTexture.h"

#include "BaseTile.h"
#include "GLUtils.h"

#define LOG_NDEBUG 1
#define LOG_TAG "BackedDoubleBufferedTexture.cpp"
#include <utils/Log.h>

namespace WebCore {

BackedDoubleBufferedTexture::BackedDoubleBufferedTexture(uint32_t w, uint32_t h,
                                                              SkBitmap::Config config)
    : DoubleBufferedTexture(eglGetCurrentContext()),
      m_canvas(0),
      m_width(0),
      m_height(0),
      m_usedLevel(-1),
      m_owner(0),
      m_painter(0),
      m_busy(false)
{
    setBitmap(w, h, config);
}

BackedDoubleBufferedTexture::~BackedDoubleBufferedTexture()
{
    m_bitmap.reset();
    delete m_canvas;
}

void BackedDoubleBufferedTexture::setBitmap(uint32_t w, uint32_t h, SkBitmap::Config config)
{
    if ((m_width == w) && (m_height == h))
        return;
    m_width = w;
    m_height = h;
    m_bitmap.reset();
    m_bitmap.setConfig(config, m_width, m_height);
    m_bitmap.allocPixels();
    m_bitmap.eraseColor(0);
    m_bitmap.eraseARGB(255, 255, 0, 0);
    delete m_canvas;
    m_canvas = new SkCanvas(m_bitmap);
}

void BackedDoubleBufferedTexture::update(TextureInfo* textureInfo, PaintingInfo& info)
{
    if (!m_width && !m_height)
        return;

    if (textureInfo && textureInfo->m_textureId) {
        if (textureInfo->m_width && textureInfo->m_height
            && (m_width != textureInfo->m_width)
            && (m_height != textureInfo->m_height)) {
            GLUtils::deleteTexture(&textureInfo->m_textureId);
            glGenTextures(1, &textureInfo->m_textureId);
            textureInfo->m_width = 0;
            textureInfo->m_height = 0;
        }

        if (!textureInfo->m_width && !textureInfo->m_height)
            GLUtils::createTextureWithBitmap(textureInfo->m_textureId, m_bitmap);
        else
            GLUtils::updateTextureWithBitmap(textureInfo->m_textureId, m_bitmap);

        if ((m_width != textureInfo->m_width)
            && (m_height != textureInfo->m_height)) {
            textureInfo->m_width = m_width;
            textureInfo->m_height = m_height;
        }
    }
    m_varLock.lock();
    if (textureInfo->m_textureId == m_textureA.getSourceTextureId())
        m_paintingInfoA = info;
    if (textureInfo->m_textureId == m_textureB.getSourceTextureId())
        m_paintingInfoB = info;
    m_varLock.unlock();
}

// Compare the current texture displayed with some PaintingInfo.
bool BackedDoubleBufferedTexture::consumerTextureUpToDate(PaintingInfo& info)
{
    android::Mutex::Autolock lock(m_varLock);
    if (getReadableTexture() == &m_textureA)
        return info == m_paintingInfoA;
    return info == m_paintingInfoB;
}

bool BackedDoubleBufferedTexture::consumerTextureSimilar(PaintingInfo& info)
{
    android::Mutex::Autolock lock(m_varLock);
    if (getReadableTexture() == &m_textureA)
        return info.similar(m_paintingInfoA);
    return info.similar(m_paintingInfoB);
}

void BackedDoubleBufferedTexture::setOwner(BaseTile* owner)
{
    android::Mutex::Autolock lock(m_varLock);
    if (m_owner != owner) {
        if (m_owner)
            m_owner->removeTexture();
        m_owner = owner;
    }
}

bool BackedDoubleBufferedTexture::acquire(BaseTile* owner)
{
    android::Mutex::Autolock lock(m_varLock);
    if (m_busy)
        return false;

    if (m_owner != owner) {
        if (m_owner)
            m_owner->removeTexture();
        m_owner = owner;
    }
    return true;
}

void BackedDoubleBufferedTexture::setPainter(BaseTile* painter)
{
    android::Mutex::Autolock lock(m_varLock);
    m_painter = painter;
}

bool BackedDoubleBufferedTexture::acquireForPainting()
{
    android::Mutex::Autolock lock(m_varLock);
    if (!m_busy) {
        m_busy = true;
        return true;
    }
    return false;
}

} // namespace WebCore
