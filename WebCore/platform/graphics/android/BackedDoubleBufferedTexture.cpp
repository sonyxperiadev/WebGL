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
    : DoubleBufferedTexture(eglGetCurrentContext())
    , m_usedLevel(-1)
    , m_owner(0)
    , m_busy(false)
{
    m_bitmap.setConfig(config, w, h);
    m_bitmap.allocPixels();
    m_bitmap.eraseColor(0);
    m_canvas = new SkCanvas(m_bitmap);
}

BackedDoubleBufferedTexture::~BackedDoubleBufferedTexture()
{
    m_bitmap.reset();
    delete m_canvas;
}

TextureInfo* BackedDoubleBufferedTexture::producerLock()
{
    m_varLock.lock();
    m_busy = true;
    m_varLock.unlock();
    return DoubleBufferedTexture::producerLock();
}

void BackedDoubleBufferedTexture::producerRelease()
{
    DoubleBufferedTexture::producerRelease();
    android::Mutex::Autolock lock(m_varLock);
    m_busy = false;
}

void BackedDoubleBufferedTexture::producerReleaseAndSwap()
{
    DoubleBufferedTexture::producerReleaseAndSwap();
    android::Mutex::Autolock lock(m_varLock);
    m_busy = false;
}

void BackedDoubleBufferedTexture::producerUpdate(BaseTile* painter,
        TextureInfo* textureInfo, PaintingInfo& info)
{
    // no need to upload a texture since the bitmap is empty
    if (!m_bitmap.width() && !m_bitmap.height()) {
        producerRelease();
        return;
    }

    if (textureInfo->m_width == m_bitmap.width() && textureInfo->m_height == m_bitmap.height())
        GLUtils::updateTextureWithBitmap(textureInfo->m_textureId, m_bitmap);
    else {
        GLUtils::createTextureWithBitmap(textureInfo->m_textureId, m_bitmap);
        textureInfo->m_width = m_bitmap.width();
        textureInfo->m_height = m_bitmap.height();
    }

    m_varLock.lock();
    // set the painting information for this texture
    if (equalsIdTextureA(textureInfo->m_textureId))
        m_paintingInfoA = info;
    else if (equalsIdTextureB(textureInfo->m_textureId))
        m_paintingInfoB = info;
    m_varLock.unlock();

    producerReleaseAndSwap();
}

// Compare the current texture displayed with some PaintingInfo.
bool BackedDoubleBufferedTexture::consumerTextureUpToDate(PaintingInfo& info)
{
    android::Mutex::Autolock lock(m_varLock);
    if (isTextureAReadable())
        return info == m_paintingInfoA;
    return info == m_paintingInfoB;
}

bool BackedDoubleBufferedTexture::consumerTextureSimilar(PaintingInfo& info)
{
    android::Mutex::Autolock lock(m_varLock);
    if (isTextureAReadable())
        return info.similar(m_paintingInfoA);
    return info.similar(m_paintingInfoB);
}

bool BackedDoubleBufferedTexture::acquire(BaseTile* owner)
{
    if (m_owner == owner)
        return true;

    // if the writable texture is busy (i.e. currently being written to) then we
    // can't change the owner out from underneath that texture
    android::Mutex::Autolock lock(m_varLock);
    if (!m_busy) {
        if (m_owner)
            m_owner->removeTexture();
        m_owner = owner;
        return true;
    }
    return false;
}

} // namespace WebCore
