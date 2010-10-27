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

class PaintingInfo {
public:
    PaintingInfo() : m_x(-1), m_y(-1), m_webview(0), m_picture(0) { }
    PaintingInfo(int x, int y, GLWebViewState* webview)
        : m_x(x)
        , m_y(y)
        , m_webview(webview)
        , m_picture(0)
    {
        if(webview)
            m_picture = webview->currentPictureCounter();
    }
    bool operator==(const PaintingInfo& info)
    {
        return m_webview == info.m_webview
            && m_x == info.m_x
            && m_y == info.m_y
            && m_picture == info.m_picture;
    }
    bool similar(const PaintingInfo& info)
    {
        return m_webview == info.m_webview
            && m_x == info.m_x
            && m_y == info.m_y;
    }
    void setPosition(int x, int y) { m_x = x; m_y = y; }
    void setGLWebViewState(GLWebViewState* webview) { m_webview = webview; }
    void setPictureUsed(unsigned int picture) { m_picture = picture; }

private:
    int m_x;
    int m_y;
    GLWebViewState* m_webview;
    unsigned int m_picture;
};

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
    void producerUpdate(BaseTile* painter, TextureInfo* textureInfo, PaintingInfo& info);

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

    // private member accessor functions
    BaseTile* owner() { return m_owner; } // only used by the consumer thread
    SkCanvas* canvas() { return m_canvas; } // only used by the producer thread

    // checks to see if the current readable texture equals the provided PaintingInfo
    bool consumerTextureUpToDate(PaintingInfo& info);
    // checks to see if the current readable texture is similar to the provided PaintingInfo
    bool consumerTextureSimilar(PaintingInfo& info);

    bool busy() { return m_busy; }

private:
    SkBitmap m_bitmap;
    SkCanvas* m_canvas;
    int m_usedLevel;
    BaseTile* m_owner;

    //The following values are shared among threads and use m_varLock to stay synced
    PaintingInfo m_paintingInfoA;
    PaintingInfo m_paintingInfoB;
    bool m_busy;

    android::Mutex m_varLock;
};

} // namespace WebCore

#endif // BackedDoubleBufferedTexture_h
