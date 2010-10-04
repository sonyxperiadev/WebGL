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
#include <SkBitmap.h>

class SkCanvas;

namespace WebCore {

class BaseTile;
class GLWebViewState;

class PaintingInfo {
public:
    PaintingInfo() : m_x(-1), m_y(-1), m_webview(0), m_picture(0) { }
    PaintingInfo(int x, int y, GLWebViewState* webview, unsigned int picture)
        : m_x(x)
        , m_y(y)
        , m_webview(webview)
        , m_picture(picture)
    {
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
    // ctor called on consumer thread
    BackedDoubleBufferedTexture(uint32_t w, uint32_t h,
                                SkBitmap::Config config = SkBitmap::kARGB_8888_Config);
    virtual ~BackedDoubleBufferedTexture();

    void setBitmap(uint32_t w, uint32_t h, SkBitmap::Config config = SkBitmap::kARGB_8888_Config);
    void update(TextureInfo* textureInfo, PaintingInfo& info);
    SkCanvas* canvas() { return m_canvas; }

    // Level can be -1 (unused texture), 0 (the 9 tiles intersecting with the
    // viewport), then the distance between the viewport and the tile.
    // we use this to prioritize which texture we want to pick (see
    // TilesManager::getAvailableTexture())
    int usedLevel() { android::Mutex::Autolock lock(m_varLock); return m_usedLevel; }
    void setUsedLevel(int used) { android::Mutex::Autolock lock(m_varLock); m_usedLevel = used; }

    BaseTile* owner() { android::Mutex::Autolock lock(m_varLock); return m_owner; }
    void setOwner(BaseTile* owner);
    BaseTile* painter() { return m_painter; }
    void setPainter(BaseTile* painter);
    bool busy() { android::Mutex::Autolock lock(m_varLock); return m_busy; }
    void setBusy(bool value) { android::Mutex::Autolock lock(m_varLock); m_busy = value; }
    uint32_t width() { return m_width; }
    uint32_t height() { return m_height; }
    bool consumerTextureUpToDate(PaintingInfo& info);
    bool consumerTextureSimilar(PaintingInfo& info);
    bool acquire(BaseTile* owner);
    bool acquireForPainting();

private:
    SkBitmap m_bitmap;
    SkCanvas* m_canvas;
    uint32_t m_width;
    uint32_t m_height;
    int m_usedLevel;
    BaseTile* m_owner;
    BaseTile* m_painter;
    PaintingInfo m_paintingInfoA;
    PaintingInfo m_paintingInfoB;
    bool m_busy;
};

} // namespace WebCore

#endif // BackedDoubleBufferedTexture_h
