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
#include "DeleteTextureOperation.h"
#include "GLUtils.h"
#include "TilesManager.h"

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
    m_size.set(w, h);
    m_bitmap.setConfig(config, w, h);
    m_bitmap.allocPixels();
    m_bitmap.eraseColor(0);
    m_canvas = new SkCanvas(m_bitmap);
}

BackedDoubleBufferedTexture::~BackedDoubleBufferedTexture()
{
    m_bitmap.reset();
    delete m_canvas;
    SharedTexture* textures[3] = { &m_textureA, &m_textureB, 0 };
    destroyTextures(textures);
}

void BackedDoubleBufferedTexture::destroyTextures(SharedTexture** textures)
{
    int x = 0;
    while (textures[x]) {
        // We need to delete the source texture and EGLImage in the texture
        // generation thread. In theory we should be able to delete the EGLImage
        // from either thread, but it currently throws an error if not deleted
        // in the same EGLContext from which it was created.
        textures[x]->lock();
        DeleteTextureOperation* operation = new DeleteTextureOperation(
            textures[x]->getSourceTextureId(), textures[x]->getEGLImage());
        textures[x]->unlock();
        TilesManager::instance()->scheduleOperation(operation);
        x++;
    }
}

TextureInfo* BackedDoubleBufferedTexture::producerLock()
{
    m_busyLock.lock();
    m_busy = true;
    m_busyLock.unlock();
    return DoubleBufferedTexture::producerLock();
}

void BackedDoubleBufferedTexture::producerRelease()
{
    DoubleBufferedTexture::producerRelease();
    android::Mutex::Autolock lock(m_busyLock);
    m_busy = false;
}

void BackedDoubleBufferedTexture::producerReleaseAndSwap()
{
    DoubleBufferedTexture::producerReleaseAndSwap();
    android::Mutex::Autolock lock(m_busyLock);
    m_busy = false;
}

bool BackedDoubleBufferedTexture::busy()
{
    android::Mutex::Autolock lock(m_busyLock);
    return m_busy;
}

void BackedDoubleBufferedTexture::producerUpdate(TextureInfo* textureInfo)
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

    producerReleaseAndSwap();
}

bool BackedDoubleBufferedTexture::acquire(TextureOwner* owner)
{
    if (m_owner == owner)
        return true;

    return setOwner(owner);
}

bool BackedDoubleBufferedTexture::setOwner(TextureOwner* owner)
{
    // if the writable texture is busy (i.e. currently being written to) then we
    // can't change the owner out from underneath that texture
    android::Mutex::Autolock lock(m_busyLock);
    if (!m_busy) {
        if (m_owner && m_owner != owner)
            m_owner->removeTexture(this);
        m_owner = owner;
        owner->addOwned(this);
        return true;
    }
    return false;
}

void BackedDoubleBufferedTexture::release(TextureOwner* owner)
{
    if (m_owner == owner) {
        m_owner->removeOwned(this);
        m_owner = 0;
    }

}

} // namespace WebCore
