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

#ifndef BaseTile_h
#define BaseTile_h

#if USE(ACCELERATED_COMPOSITING)

#include "BackedDoubleBufferedTexture.h"
#include "HashMap.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkRect.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

namespace WebCore {

class TiledPage;

class BaseTile {
public:
#ifdef DEBUG_COUNT
    static int count();
#endif
    BaseTile(TiledPage* page, int x, int y, int quality = 1);
    ~BaseTile();

    void reserveTexture();
    void removeTexture();
    void setUsedLevel(int);
    bool paintBitmap();
    bool isBitmapReady();
    float scale() const { return m_scale; }
    void setScale(float scale) { m_scale = scale; }
    void draw(float transparency, SkRect& rect);
    TiledPage* page() { return m_page; }
    int quality() const { return m_quality; }
    int x() const { return m_x; }
    int y() const { return m_y; }
    BackedDoubleBufferedTexture* texture() { return m_texture; }

private:
    TiledPage* m_page;
    BackedDoubleBufferedTexture* m_texture;
    int m_x;
    int m_y;
    int m_quality;
    float m_scale;
    android::Mutex m_varLock;
};

typedef std::pair<int, int> TileKey;
typedef HashMap<TileKey, BaseTile*> TileMap;

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // BaseTile_h
