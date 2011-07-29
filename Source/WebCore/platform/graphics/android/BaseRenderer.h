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

#ifndef BaseRenderer_h
#define BaseRenderer_h

#if USE(ACCELERATED_COMPOSITING)

#include "PerformanceMonitor.h"
#include "SkRect.h"

class SkCanvas;
class SkDevice;

namespace WebCore {

class TextureInfo;
class TilePainter;
class BaseTile;

struct TileRenderInfo {
    // coordinates of the tile
    int x;
    int y;

    // current scale factor
    float scale;

    // inval rectangle with coordinates in the tile's coordinate space
    SkIRect* invalRect;

    // the expected size of the tile
    SkSize tileSize;

    // the painter object in charge of drawing our content
    TilePainter* tilePainter;

    // the base tile calling us
    BaseTile* baseTile;

    // info about the texture that we are to render into
    TextureInfo* textureInfo;

    // specifies whether or not to measure the rendering performance
    bool measurePerf;
};

/**
 *
 */
class BaseRenderer {
public:
    enum RendererType { Raster, Ganesh };
    BaseRenderer(RendererType type) : m_type(type) {}
    virtual ~BaseRenderer() {}

    int renderTiledContent(const TileRenderInfo& renderInfo);

    RendererType getType() { return m_type; }

    static BaseRenderer* createRenderer();
    static void swapRendererIfNeeded(BaseRenderer*& renderer);
    static RendererType getCurrentRendererType() { return g_currentType; }
    static void setCurrentRendererType(RendererType type) { g_currentType = type; }

protected:

    virtual void setupCanvas(const TileRenderInfo& renderInfo, SkCanvas* canvas) = 0;
    virtual void setupPartialInval(const TileRenderInfo& renderInfo, SkCanvas* canvas) {}
    virtual void renderingComplete(const TileRenderInfo& renderInfo, SkCanvas* canvas) = 0;

    void drawTileInfo(SkCanvas* canvas, const TileRenderInfo& renderInfo,
            int pictureCount);

    virtual const String* getPerformanceTags(int& tagCount) = 0;

    // Performance tracking
    PerformanceMonitor m_perfMon;

private:
    RendererType m_type;
    static RendererType g_currentType;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // BaseRenderer_h
