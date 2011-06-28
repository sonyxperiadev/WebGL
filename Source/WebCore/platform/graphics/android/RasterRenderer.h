/*
 * Copyright 2011, The Android Open Source Project
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

#ifndef RasterRenderer_h
#define RasterRenderer_h

#if USE(ACCELERATED_COMPOSITING)

#include "PerformanceMonitor.h"
#include "SkRect.h"

class SkCanvas;

namespace WebCore {

class BaseTileTexture;
class TextureInfo;
class TiledPage;

/**
 *
 */
class RasterRenderer {
public:
    RasterRenderer();
    ~RasterRenderer();

    void drawTileInfo(SkCanvas* canvas,
                      BaseTileTexture* texture,
                      TiledPage* tiledPage,
                      int x, int y, float scale, int pictureCount);

    int renderContent(int x, int y, SkIRect rect, float tx, float ty,
                      float scale, BaseTileTexture* texture,
                      TextureInfo* textureInfo,
                      TiledPage* tiledPage,
                      bool fullRepaint);

private:

    // Performance tracking
    PerformanceMonitor m_perfMon;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // RasterRenderer_h
