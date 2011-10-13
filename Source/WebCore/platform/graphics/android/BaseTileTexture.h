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

#ifndef BaseTileTexture_h
#define BaseTileTexture_h

#include "DoubleBufferedTexture.h"
#include "GLWebViewState.h"
#include "TextureOwner.h"
#include "TilePainter.h"
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
    , m_painter(0)
    , m_picture(0)
    , m_inverted(false)
    {
    }
    int m_x;
    int m_y;
    int m_layerId;
    float m_scale;
    TextureInfo* m_texture;
    TilePainter* m_painter;
    unsigned int m_picture;
    bool m_inverted;
};

// DoubleBufferedTexture using a SkBitmap as backing mechanism
class BaseTileTexture : public DoubleBufferedTexture {
public:
    // This object is to be constructed on the consumer's thread and must have
    // a width and height greater than 0.
    BaseTileTexture(uint32_t w, uint32_t h);
    virtual ~BaseTileTexture();

    // these functions override their parent
    virtual TextureInfo* producerLock();
    virtual void producerRelease();
    virtual void producerReleaseAndSwap();

    // updates the texture with current bitmap and releases (and if needed also
    // swaps) the texture.
    virtual void producerUpdate(TextureInfo* textureInfo, const SkBitmap& bitmap);

    // allows consumer thread to assign ownership of the texture to the tile. It
    // returns false if ownership cannot be transferred because the tile is busy
    bool acquire(TextureOwner* owner, bool force = false);
    bool release(TextureOwner* owner);

    // removes Tile->Texture, and Texture->Tile links to fully discard the texture
    void releaseAndRemoveFromTile();

    // set the texture owner if not busy. Return false if busy, true otherwise.
    bool setOwner(TextureOwner* owner, bool force = false);

    // private member accessor functions
    TextureOwner* owner() { return m_owner; } // only used by the consumer thread

    bool busy();
    void setNotBusy();

    const SkSize& getSize() const { return m_size; }

    void setTile(TextureInfo* info, int x, int y, float scale,
                 TilePainter* painter, unsigned int pictureCount);
    bool readyFor(BaseTile* baseTile);
    float scale();

    // OpenGL ID of backing texture, 0 when not allocated
    GLuint m_ownTextureId;
    // these are used for dynamically (de)allocating backing graphics memory
    void requireGLTexture();
    void discardGLTexture();

    void setOwnTextureTileInfoFromQueue(const TextureTileInfo* info);

protected:
    HashMap<SharedTexture*, TextureTileInfo*> m_texturesInfo;

private:
    void destroyTextures(SharedTexture** textures);
    TextureTileInfo m_ownTextureTileInfo;

    SkSize m_size;
    SkBitmap::Config m_config;

    // BaseTile owning the texture, only modified by UI thread
    TextureOwner* m_owner;

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

#endif // BaseTileTexture_h
