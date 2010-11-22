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

#ifndef BackedDoubleBufferedTexture_h
#define BackedDoubleBufferedTexture_h

#include "DoubleBufferedTexture.h"
#include "GLWebViewState.h"
#include <SkBitmap.h>

class SkCanvas;

namespace WebCore {

class BaseTile;

// DoubleBufferedTexture using a SkBitmap as backing mechanism
class BackedDoubleBufferedTexture : public DoubleBufferedTexture {
public:
    // This object is to be constructed on the consumer's thread and must have
    // a width and height greater than 0.
    BackedDoubleBufferedTexture(uint32_t w, uint32_t h,
                                SkBitmap::Config config = SkBitmap::kARGB_8888_Config);
    virtual ~BackedDoubleBufferedTexture();

    // these functions override their parent
    virtual TextureInfo* producerLock();
    virtual void producerRelease();
    virtual void producerReleaseAndSwap();

    // updates the texture with current bitmap and releases (and if needed also
    // swaps) the texture.
    void producerUpdate(TextureInfo* textureInfo);

    // The level can be one of the following values:
    //  * -1 for an unused texture.
    //  *  0 for the tiles intersecting with the viewport.
    //  *  n where n > 0 for the distance between the viewport and the tile.
    // We use this to prioritize the order in which we reclaim textures, see
    // TilesManager::getAvailableTexture() for more information.
    int usedLevel() { return m_usedLevel; }
    void setUsedLevel(int used) { m_usedLevel = used; }

    // allows consumer thread to assign ownership of the texture to the tile. It
    // returns false if ownership cannot be transferred because the tile is busy
    bool acquire(BaseTile* owner);
    void release(BaseTile* owner);

    // private member accessor functions
    BaseTile* owner() { return m_owner; } // only used by the consumer thread
    SkCanvas* canvas() { return m_canvas; } // only used by the producer thread

    // This is to be only used for debugging on the producer thread
    bool busy() { return m_busy; }

private:
    SkBitmap m_bitmap;
    SkCanvas* m_canvas;
    int m_usedLevel;
    BaseTile* m_owner;

    // This values signals that the texture is currently in use by the consumer.
    // This allows us to prevent the owner of the texture from changing while the
    // consumer is holding a lock on the texture.
    bool m_busy;
    // We mutex protect the reads/writes of m_busy to ensure that we are reading
    // the most up-to-date value even across processors in an SMP system.
    android::Mutex m_busyLock;
};

} // namespace WebCore

#endif // BackedDoubleBufferedTexture_h
