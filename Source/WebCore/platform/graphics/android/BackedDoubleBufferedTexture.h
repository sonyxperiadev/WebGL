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
#include "TextureOwner.h"
#include <SkBitmap.h>

class SkCanvas;

namespace WebCore {

class BaseTile;

class TextureTileInfo {
 public:
  TextureTileInfo()
      : m_x(-1)
      , m_y(-1)
      , m_layerId(-1)
      , m_scale(0)
      , m_texture(0)
      , m_picture(0)
  {
  }
  int m_x;
  int m_y;
  int m_layerId;
  float m_scale;
  TextureInfo* m_texture;
  unsigned int m_picture;
};

// DoubleBufferedTexture using a SkBitmap as backing mechanism
class BackedDoubleBufferedTexture : public DoubleBufferedTexture {
public:
    // This object is to be constructed on the consumer's thread and must have
    // a width and height greater than 0.
    BackedDoubleBufferedTexture(uint32_t w, uint32_t h,
                                SkBitmap* bitmap = 0,
                                SkBitmap::Config config = SkBitmap::kARGB_8888_Config);
    virtual ~BackedDoubleBufferedTexture();

    // these functions override their parent
    virtual TextureInfo* producerLock();
    virtual void producerRelease();
    virtual void producerReleaseAndSwap();

    // updates the texture with current bitmap and releases (and if needed also
    // swaps) the texture.
    virtual void producerUpdate(TextureInfo* textureInfo);
    void producerUpdate(TextureInfo* textureInfo, SkBitmap* bitmap, SkIRect& rect);
    bool textureExist(TextureInfo* textureInfo);

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
    bool acquire(TextureOwner* owner, bool force = false);
    bool release(TextureOwner* owner);
    bool tryAcquire(TextureOwner* owner, TiledPage* currentPage, TiledPage* nextPage);

    // set the texture owner if not busy. Return false if busy, true otherwise.
    bool setOwner(TextureOwner* owner, bool force = false);

    // private member accessor functions
    TextureOwner* owner() { return m_owner; } // only used by the consumer thread
    TextureOwner* delayedReleaseOwner() { return m_delayedReleaseOwner; }
    SkCanvas* canvas(); // only used by the producer thread
    SkBitmap* bitmap() { return m_bitmap; }

    bool busy();
    void setNotBusy();

    const SkSize& getSize() const { return m_size; }

    void setTile(TextureInfo* info, int x, int y, float scale, unsigned int pictureCount);
    bool readyFor(BaseTile* baseTile);

protected:
    HashMap<SharedTexture*, TextureTileInfo*> m_texturesInfo;

private:
    void destroyTextures(SharedTexture** textures);

    SkBitmap* m_bitmap;
    bool m_sharedBitmap;
    SkSize m_size;
    SkCanvas* m_canvas;
    int m_usedLevel;
    SkBitmap::Config m_config;
    TextureOwner* m_owner;

    // When trying to release a texture, we may delay this if the texture is
    // currently used (busy being painted). We use the following two variables
    // to do so in setNotBusy()
    TextureOwner* m_delayedReleaseOwner;
    bool m_delayedRelease;

    // This values signals that the texture is currently in use by the consumer.
    // This allows us to prevent the owner of the texture from changing while the
    // consumer is holding a lock on the texture.
    bool m_busy;
    // We mutex protect the reads/writes of m_busy to ensure that we are reading
    // the most up-to-date value even across processors in an SMP system.
    android::Mutex m_busyLock;
    // We use this condition variable to signal that the texture
    // is not busy anymore
    android::Condition m_busyCond;
};

} // namespace WebCore

#endif // BackedDoubleBufferedTexture_h
